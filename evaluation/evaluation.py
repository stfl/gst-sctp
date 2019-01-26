import argparse
import numpy as np
#  import numpy.lib.recfunctions as rcfuncs
import pandas as pd
import statistics as st

import shelve
from copy import deepcopy, copy
import sys
import os
import glob
import re
from operator import attrgetter
import pyshark

from IPython import embed
from IPython.core import ultratb
sys.excepthook = ultratb.FormattedTB(mode='Verbose', color_scheme='Linux', call_pdb=1)

from eval_exceptions import InvalidValue, ReceiverKilled
#  from experiment import Experiment
#  from run import Run

#  from mpl_toolkits.axes_grid1 import host_subplot
#  import mpl_toolkits.axisartist as aa

import matplotlib
import matplotlib.pyplot as plt
#  matplotlib.style.use('ggplot')
matplotlib.style.use('fivethirtyeight')
plt.rcParams['axes.facecolor'] = 'white'
plt.rcParams['axes.edgecolor'] = 'none'
plt.rcParams['savefig.facecolor'] = 'white'
plt.rcParams['savefig.edgecolor'] = 'none'

#  from scipy.interpolate import UnivariateSpline
#  import math

# config
fps = 24
init_offset = 3 * fps  # frames
num_packets_per_frame = 11
payload_length = 1400 - 12  # MTU - RTP header
drop_correlation = .25
gst_sched_delay = 1. / fps  # in S
cutoff_init = int(init_offset * num_packets_per_frame)
cutoff_end = num_packets_per_frame  # 1 frame
pf_time_measure = 3
rtx_deadline = 52  # ms  (D + gst_sched_delay)/3

# Headers for Transport Overhead
header_eth = 12
header_ip = 20
header_rtp = 12
header_sctp_common = 12
header_sum_common_sctp = header_sctp_common + header_rtp + header_ip + header_eth

header_udp = 8
header_sum_upd = header_udp + header_rtp + header_ip + header_eth
packet_udp = header_sum_upd + payload_length

header_sctp_data = 16
packet_sctp = header_sctp_data + header_sum_common_sctp + payload_length

chunk_nr_sack_fast = 20 + 1 * 4  # NR-SACK with average 1x GabACK Group
packet_nr_sack_fast = chunk_nr_sack_fast + header_sum_common_sctp
chunk_nr_sack_slow = 20 + 6 * 4  # NR-SACK with average 2x GabACK Group
packet_nr_sack_slow = chunk_nr_sack_slow + header_sum_common_sctp

chunk_nr_sack_slow_dpr = 20 + 4 * 4  # NR-SACK with average 2x GabACK Group
packet_nr_sack_slow_dpr = chunk_nr_sack_slow_dpr + header_sum_common_sctp

chunk_fwd = 12
packet_fwd = chunk_fwd + header_sum_common_sctp

print("udp", packet_udp)
print("sctp DATA", packet_sctp)
print("sctp FWD", packet_fwd)
print("sctp NR-SACK", packet_nr_sack_fast, packet_nr_sack_slow)


plot_colors = ['maroon', 'red', 'olive', 'yellow', 'green', 'lime', 'teal', 'orange', 'aqua', 'navy',
               'blue', 'purple', 'fuchsia', 'maroon', 'green']

plot_colors_variant = {'single': 'black',
                       'cmt': 'green',
                       'dpr': 'blue',
                       'dpr_acc': 'purple',
                       'dupl': 'gray',
                       'udp': 'orange',
                       'udpdupl': 'red',
                       }

plot_markers_variant = {'single': 'o',
                        'cmt': 's',
                        'dpr': '^',
                        'dpr_acc': 'v',
                        'dupl': '.',
                        'udp': 'P',
                        'udpdupl': 'X',
                        }

class Experiment:
    '''an Experiment is a set of runs with the same characteristics, like delay, packet drop rate
    and the variant used set.'''

    def __init__(self, run_path, results_dir):
        self.run_path = run_path
        self.results_dir = results_dir
        self.runs = []
        self.num_runs = results_dir.num_runs
        self.__collect_runs()
        self.accomodating_gst_delay = False

        if self.num_runs > 0:
            self.trace = pd.concat([r.trace for r in self.runs])

            assert len(self.trace) + self.lost == self.packets_sent_stream
        #  self.all_ttd_jb = np.concatenate([r.trace_out['ttd'] for r in self.runs])
        #  assert len(self.all_ttd_jb) + self.lost == self.packets_sent_stream

    def __collect_runs(self):
        with open(os.path.join(self.run_path, "run_config")) as run_config_file:
            run_config = run_config_file.read()

            match = re.search(r"variant=(\S+)", run_config)
            assert match is not None
            self.variant = match[1]

            match = re.search(r'delay_on_link=(\d+)', run_config)
            assert match is not None
            self.delay = int(match[1])

            match = re.search(r"drop_rate=([\d.]+)", run_config)
            assert match is not None
            self.drop_rate = float(match[1])

        for run_id in range(1, self.num_runs+1):
            try:
                self.runs.append(Run(self.run_path, run_id, self.results_dir, self.variant, self.drop_rate, self.delay))
            except (InvalidValue, ReceiverKilled):
                #  print("Ignoring this run")
                self.num_runs -= 1
                continue
        assert len(self.runs) == self.num_runs

    @property
    def deadline(self):
        return self.results_dir.deadline

    #  def cdf_in(self, variant):
    @property
    def ddr(self):
        return float(self.deadline_hit / self.packets_sent_stream)

    @property
    def packets_sent_stream(self):
        return sum(r.packets_sent_stream for r in self.runs)

    @property
    def packets_sent_stream_unmasked(self):
        return sum(r.packets_sent_stream_unmasked for r in self.runs)

    @property
    def packets_sent_link(self):
        return sum(r.packets_sent_link for r in self.runs)

    @property
    def deadline_hit(self):
        #  return sum(r.deadline_hit for r in self.runs)
        if self.accomodating_gst_delay is True:
            return len(self.trace.query('ttd <= ((@self.deadline * 1000000) + @gst_sched_delay * 1000000000)'))
        else:
            return len(self.trace.query('ttd <= (@self.deadline * 1000000)'))

    @property
    def lost(self):
        return sum(r.lost for r in self.runs)

    @property
    def deadline_miss(self):
        return sum(r.deadline_miss for r in self.runs)

    @property
    def duplicates(self):
        return self.duplicates_unmasked

    @property
    def duplicates_unmasked(self):
        return sum(r.duplicates_unmasked for r in self.runs)

    @property
    def ttd_max(self):
        return self.trace.ttd.max()

    @property
    def ttd_mean(self):
        return self.trace.ttd.mean()

    @property
    def ttd_median(self):
        return self.trace.ttd.median()

    @property
    def ttd_std(self):
        return self.trace.ttd.std()

    @property
    def ttd_var(self):
        return self.trace.ttd.var()

    @property
    def sender_tx(self):
        return [sum(r.sender_tx[0] for r in self.runs), sum(r.sender_tx[1] for r in self.runs)]

    #  def hist(self):
    #      return eval_hist(self.trace.ttd/1000000, min(int(self.ttd_max / 1000000 / 5), 50))

    def ttd_quantile(self, q):
        return self.trace.ttd.quantile(q, interpolation='lower')

    @property
    def tro(self):
        return self.tro_bytes
        #  return self.packets_sent_link / float(self.packets_sent_stream_unmasked - self.sender_buffer_blocked) - 1

    # netem drops the packets before they are counted towards bytes_rx. The actual value is adjusted
    # with the configured drop rate
    @property
    def tro_bytes(self):
        bytes_sent_full = self.bytes_sent_link / (1 - (1 + drop_correlation +
                                                       drop_correlation ** 2 +
                                                       drop_correlation ** 3)
                                                  * self.drop_rate / 100)
        return (bytes_sent_full / self.bytes_sent_stream) - 1

    @property
    def tro_expected(self):
        drop_actual = (1 + drop_correlation) * self.drop_rate / 100

        if self.variant == 'udp':
            return packet_udp / payload_length - 1
        elif self.variant == 'udpdupl':
            return 2 * packet_udp / payload_length - 1
        elif self.delay < (self.deadline - gst_sched_delay * 1000) / 3:
                return ((packet_sctp
                         + packet_nr_sack_fast
                         + packet_sctp * drop_actual  # fast retransmissions
                         + chunk_fwd * drop_actual  # / num_packets_per_frame
                         ) / payload_length) - 1
        else:
            if self.variant == 'dpr' or self.variant == 'dpr_acc':
                return ((packet_sctp * 2
                         + packet_nr_sack_slow
                         + chunk_fwd  # / num_packets_per_frame
                         ) / payload_length) - 1
            else:
                return ((packet_sctp
                         + packet_nr_sack_slow * (1 - drop_actual)
                         + chunk_fwd  # / num_packets_per_frame
                         ) / payload_length) - 1
        return 0


    @property
    def bytes_sent_link(self):
        return sum(r.bytes_sent_link for r in self.runs)

    @property
    def bytes_sent_stream(self):
        return sum(r.bytes_sent_stream for r in self.runs)

    @property
    def sender_buffer_blocked(self):
        return sum(r.sender_buffer_blocked for r in self.runs)

    @property
    def num_frames(self):
        return self.results_dir.num_frames

    @property
    def ddr_expected(self):
        if self.delay < (self.deadline - gst_sched_delay * 1000) / 3:
            if self.variant == 'udp':
                return 1 - (self.drop_rate * (1 + drop_correlation)/100)
            else:  # single, cmt, dpr, dpr_acc, udpdupl
                return 1 - (self.drop_rate * (1 + drop_correlation)/100) ** 2
        else:
            if self.variant in ['udp', 'single', 'cmt']:
                return 1 - (self.drop_rate * (1 + drop_correlation)/100)
            else:  # dpr, dpr_acc, udpdupl
                return 1 - (self.drop_rate * (1 + drop_correlation)/100) ** 2

    def __str__(self):
        q1, q2 = self.ttd_quantile([.8, .95]) / 1000000
        exp_str = ("Variant:{variant} delay:{delay}ms drop:{drop:.1f}%({drop_full:.1f}%) D:{deadline}ms frames:{frames} {runs}runs\n"
                   "sent:{sent:5d} hit:{hit:5d} miss:{miss:4d} lost:{lost:4d} ({ml:4d}) DDR:{ddr:.2%}\n"
                   "TTD mean:{mean:6.2f} std:{std:6.2f} max:{max:6.2f} q80:{q1:6.2f} q95:{q2:6.2f}\n"
                   "link:{link:6d} dupl:{dupl:5d} TO:{tro_bytes:.2%} TOe:{tro_exp:.2%} s_blk:{send_block} tr: {s1:.0f}/{s2:.0f}kB\n"
                   #  "rtx:{rtx}, abnd:{abandoned}\n"
                   .format(delay=self.delay,
                           deadline=self.deadline,
                           drop=self.drop_rate,
                           drop_full=self.drop_rate * (1 + drop_correlation),
                           variant=self.variant,
                           runs=self.num_runs,
                           frames=self.results_dir.num_frames,
                           ddr=self.ddr,
                           sent=self.packets_sent_stream,
                           hit=self.deadline_hit,
                           miss=self.deadline_miss,
                           lost=self.lost,
                           link=self.packets_sent_link,
                           ml=self.deadline_miss + self.lost,
                           dupl=self.duplicates_unmasked,
                           tro=self.tro,
                           tro_bytes=self.tro_bytes,
                           tro_exp=self.tro_expected,
                           mean=self.ttd_mean/1000000,
                           std=self.ttd_std/1000000,
                           max=self.ttd_max/1000000,
                           q1=q1, q2=q2,
                           send_block=self.sender_buffer_blocked,
                           s1=self.sender_tx[0]/1000,
                           s2=self.sender_tx[1]/1000
                           #  abandoned=self.abandoned if self.variant not in ('udp', 'udpdupl') else 0,
                           #  rtx=self.sender_rtx_data if self.variant not in ('udp', 'udpdupl') else 0
                           ))

        return exp_str


class Run():
    '''A Run is a single run with a defined number of frames transfered through gstreamer'''
    def __init__(self, run_path, run_id, results_dir, variant, drop_rate, delay):
        self.run_path = run_path
        self.run_id = run_id
        self.variant = variant
        self.drop_rate = drop_rate
        self.delay = delay
        self.results_dir = results_dir
        self.duplicates_unmasked = 0
        self.packets_sent_stream_unmasked = self.results_dir.num_frames * num_packets_per_frame
        self.packets_sent_stream = self.packets_sent_stream_unmasked - cutoff_init - cutoff_end
        self.receiver_killed = os.path.isfile(os.path.join(self.run_path, 'receiver_killed_' + str(self.run_id)))
        self.pf_last_seqnum = -1


        if self.receiver_killed:
            print('receiver killed')
            raise ReceiverKilled

        file_experiment_trace_in = os.path.join(self.run_path, 'experiment_trace_in_' + str(self.run_id) + '.csv')
        if not os.path.isfile(file_experiment_trace_in):
            print("trace not found")
            raise InvalidValue

        #  file_experiment_trace_out = os.path.join(self.run_path, 'experiment_trace_out_' + str(self.run_id) + '.csv')
        #  self.trace = np.genfromtxt(file_experiment_trace_in, names=True, delimiter=';', dtype=np.int64)
        #  self.trace_jb = np.genfromtxt(self.file_experiment_trace_out, names=True, delimiter=';', dtype=int)
        self.trace = pd.read_csv(file_experiment_trace_in, delimiter=';')

        # XXX should be a parameter in ResultsDir
        if args.pathfailure is True:
            cap = pyshark.FileCapture(os.path.join(self.run_path, 'receiver_' + str(self.run_id) + '.pcap'),
                                      decode_as={'sctp.ppi==99': 'asap', 'udp.port==55555': 'rtp'})
            #  cap.set_debug()
            cap.load_packets()
            #  embed()
            for p in reversed(cap):
                if 'asap' in p and 'message_length' in p.asap.field_names:
                    # asap message length overlaps with RTP seqnum
                    seqnum = int(p.asap.message_length)

                    #  print('last data packet on failed path: src:', p.ip.src, 'TSN:',
                    #        p.sctp.data_tsn, 'Seqnum:', p.asap.message_length)
                elif 'rtp' in p:
                    seqnum = int(p.rtp.seq)
                else:
                    continue

                if seqnum > self.pf_last_seqnum:
                    self.pf_last_seqnum = seqnum
                elif seqnum < self.pf_last_seqnum - 50:
                    # don't search too long
                    break

            self.packets_sent_stream = pf_time_measure * fps * num_packets_per_frame
            self.trace_full = self.trace.copy()

            self.start_time = min(self.trace_full['rtptime'])
            #  print(self.trace_full[self.trace_full.seqnum == self.pf_last_seqnum]['rtptime'])
            # there might have been duplicates
            self.pf_rtptime = min(self.trace_full[self.trace_full.seqnum == self.pf_last_seqnum]['rtptime'])
            self.pf_time = self.pf_rtptime - self.start_time
            if self.pf_time < 3:
                print("pf_time seems to be wrong %d" % self.pf_time)
                raise InvalidValue

            self.trace.drop_duplicates('seqnum', keep='first', inplace=True)  # keeps the first, by index! (which is the lower "now")
            self.trace = self.trace[(self.trace.rtptime > self.pf_rtptime) &
                                    (self.trace.rtptime <= self.pf_rtptime + pf_time_measure * 1000000000)]

            # show packets after path failure time
            #  if self.variant == "udp" or self.variant == "single":
            #      print(self.trace[self.trace.rtptime > self.pf_rtptime])

            print(self.variant, "last:", self.pf_last_seqnum,
                  "t_pf:", self.pf_time/1000000000,
                  "cutoff:", self.pf_last_seqnum + self.packets_sent_stream,
                  "trace:", len(self.trace), "/", self.packets_sent_stream)

            self.trace['ttd'] = self.trace.now - self.trace.rtptime
        else:
            # cut off init phase and end phase

            self.trace = self.trace[(self.trace.seqnum >= cutoff_init) &
                                    (self.trace.seqnum < (self.packets_sent_stream_unmasked - cutoff_end))]

            len_trace_all = len(self.trace)
            self.trace.drop_duplicates('seqnum', keep='first', inplace=True)  # keeps the first, by index! (which is the lower "now")
            self.trace['ttd'] = self.trace.now - self.trace.rtptime

            if len(self.trace) < (cutoff_init + cutoff_end):
                print("trace too short %d" % len(self.trace))
                raise InvalidValue

            self.start_time = min(self.trace['rtptime'])
            if min(self.trace.ttd) <= 0:
                print("negative ttd ", min(self.trace.ttd), " in ", file_experiment_trace_in)
                #  fig, ax = plt.subplots()
                #  df = pd.DataFrame(self.trace, columns=['rtptime', 'now']) / 1000000
                #  df.plot(sharey=True, ax=ax)
                #  (self.trace.ttd / 1000000).plot(secondary_y=True, label='TTD', legend=True, mark_right=False)
                #  ax.set_xlabel('Sequencenumer')
                #  ax.set_ylabel('Timestamp [ms]')
                #  ax.right_ax.set_ylabel('TTD [ms]')
                #  plt.show()

                raise InvalidValue
        #  assert min(self.trace.ttd) > 0

        # calc DDR
        self.deadline_hit = len(self.trace.query('now < deadline'))
        self.lost = self.packets_sent_stream - len(self.trace)
        self.deadline_miss = len(self.trace) - self.deadline_hit
        self.ddr = float(self.deadline_hit / self.packets_sent_stream)

        with open(os.path.join(self.run_path, "out_sender_" + str(self.run_id))) as out_sender_file:
            out_sender = out_sender_file.read()
            self.sender_buffer_blocked = len(re.findall(r"usrsctp_sendv failed:", out_sender))
            if self.sender_buffer_blocked > self.packets_sent_stream_unmasked * 0.2:
                # ignore if more than 20% are sender buffer blocked
                print('too much sender buffer blocking %d/%d' % (self.sender_buffer_blocked, self.packets_sent_stream_unmasked))
                raise InvalidValue

        self.__calc_transfered_bytes()
        if self.variant in ('dpr', 'cmt', 'dupl', 'udpdupl') and args.pathfailure is False:
            if (min(self.sender_tx[0], self.sender_tx[1]) * 5 < max(self.sender_tx[0], self.sender_tx[1])):
                print('multihoming seams not to be used. phy1:{:.0f}kB phy2:{:.0f}kB  diff: {:.0f}kB'
                      .format(self.sender_tx[0]/1000, self.sender_tx[1]/1000, abs(self.sender_tx[0] - self.sender_tx[1])/1000))
                raise InvalidValue

        self.bytes_sent_stream = self.packets_sent_stream_unmasked * payload_length
        self.bytes_sent_link = sum(self.sender_tx) + sum(self.receiver_tx)

        #  print("sender tx: %d %d\t rx: %d %d" % (self.sender_tx[0], self.sender_tx[1], self.sender_rx[0], self.sender_rx[1]))
        #  print("receiv rx: %d %d\t tx: %d %d" % (self.receiver_rx[0], self.receiver_rx[1], self.receiver_tx[0], self.receiver_tx[1]))

        if self.variant == 'udp':
            self.packets_sent_link = self.packets_sent_stream_unmasked
        elif self.variant == 'udpdupl':
            self.packets_sent_link = self.packets_sent_stream_unmasked * 2
        else:
            self.__read_usrsctp_stats()
            #  print(self.bytes_sent_stream, self.bytes_sent_link, (self.bytes_sent_link * (self.drop_rate / 100 + 1) / self.bytes_sent_stream) - 1, self.abandoned, self.sender_rtx_data)

        #  if self.tro < 0:
        #      print("negative tro stream %d, link %d, blk %d (%.2f) abandoned %d" %
        #            (self.packets_sent_stream_unmasked, self.packets_sent_link,
        #             self.sender_buffer_blocked, self.tro, self.abandoned))
            #  raise InvalidValue
            #  import ipdb; ipdb.set_trace()

    def __read_usrsctp_stats(self):
        with open(os.path.join(self.run_path, "usrsctp_stats_receiver_" + str(self.run_id))) as usrsctp_stats_receiver_file:
            usrsctp_stats_receiver = usrsctp_stats_receiver_file.read()

            match = re.search(r"sent_packets=(\d+)", usrsctp_stats_receiver)
            assert match is not None
            self.receiver_sent_packets = int(match[1])

            match = re.search(r"recv_packets=(\d+)", usrsctp_stats_receiver)
            assert match is not None
            self.receiver_recv_packets = int(match[1]) - 1  # the first one is a dummy

            match = re.search(r"recv_data=(\d+)", usrsctp_stats_receiver)
            assert match is not None
            self.receiver_recv_data = int(match[1]) - 1  # the first one is a dummy

            match = re.search(r"recv_dupdata=(\d+)", usrsctp_stats_receiver)
            assert match is not None
            self.receiver_recv_dupdata = int(match[1])
            self.duplicates_unmasked += self.receiver_recv_dupdata

            match = re.search(r"send_sacks=(\d+)", usrsctp_stats_receiver)
            assert match is not None
            self.receiver_send_sacks = int(match[1])

            match = re.search(r"recv_sacks=(\d+)", usrsctp_stats_receiver)
            assert match is not None
            self.receiver_recv_sacks = int(match[1])

        with open(os.path.join(self.run_path, "usrsctp_stats_sender_" + str(self.run_id))) as usrsctp_stats_sender_file:
            usrsctp_stats_sender = usrsctp_stats_sender_file.read()

            match = re.search(r"sent_packets=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_sent_packets = int(match[1]) - 1  # the first one is a dummy

            match = re.search(r"recv_packets=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_recv_packets = int(match[1])

            match = re.search(r"send_data=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_send_data = int(match[1]) - 1  # the first one is a dummy

            match = re.search(r"send_sacks=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_send_sacks = int(match[1])

            match = re.search(r"recv_sacks=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_recv_sacks = int(match[1])

            match = re.search(r"send_retrans_data=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_rtx_data = int(match[1])

            #  match = re.search(r"send_fast_retrans=(\d+)", usrsctp_stats_sender)
            #  assert match is not None
            #  self.sender_frtx = int(match[1])

            self.packets_sent_link = self.sender_send_data + self.sender_rtx_data

            #  timer_dpr_fired=189
            #  dpr_avg_delay_timer=45552
            #  dpr_flagged=0

            match = re.search(r"abandoned_sent=(\d+)", usrsctp_stats_sender)
            assert match is not None
            sender_abandoned_sent = int(match[1])

            match = re.search(r"abandoned_unsent=(\d+)", usrsctp_stats_sender)
            assert match is not None
            sender_abandoned_unsent = int(match[1])
            self.abandoned = sender_abandoned_sent + sender_abandoned_unsent

            if self.variant != 'dupl' and self.abandoned > 0.5 * self.packets_sent_stream_unmasked:
                print("too many abandoned %d/%d" % (self.abandoned, self.packets_sent_stream_unmasked))
                raise InvalidValue

    def __calc_transfered_bytes(self):
        with open(os.path.join(self.run_path, "sender_transfered_bytes_" + str(self.run_id))) as sender_tr_file:
            sender = sender_tr_file.read()
            match = re.search(r"sender_tx_phy1=(\d+)", sender)
            assert match is not None
            self.sender_tx = [int(match[1])]

            match = re.search(r"sender_tx_phy2=(\d+)", sender)
            assert match is not None
            self.sender_tx.append(int(match[1]))

            self.sender_rx = []
            match = re.search(r"sender_rx_phy1=(\d+)", sender)
            assert match is not None
            self.sender_rx = [int(match[1])]

            match = re.search(r"sender_rx_phy2=(\d+)", sender)
            assert match is not None
            self.sender_rx.append(int(match[1]))

        with open(os.path.join(self.run_path, "receiver_transfered_bytes_" + str(self.run_id))) as receiver_tr_file:
            receiver = receiver_tr_file.read()
            match = re.search(r"receiver_tx_phy1=(\d+)", receiver)
            assert match is not None
            self.receiver_tx = [int(match[1])]

            match = re.search(r"receiver_tx_phy2=(\d+)", receiver)
            assert match is not None
            self.receiver_tx.append(int(match[1]))

            self.receiver_rx = []
            match = re.search(r"receiver_rx_phy1=(\d+)", receiver)
            assert match is not None
            self.receiver_rx = [int(match[1])]

            match = re.search(r"receiver_rx_phy2=(\d+)", receiver)
            assert match is not None
            self.receiver_rx.append(int(match[1]))

    @property
    def duplicates(self):
        return self.duplicates_unmasked

    @property
    def ttd_max(self):
        return self.trace['ttd'].max()  # ns

    @property
    def ttd_mean(self):
        return self.trace['ttd'].mean()

    @property
    def ttd_std(self):
        return self.trace['ttd'].std()

    @property
    def ttd_var(self):
        return self.trace['ttd'].var()

    @property
    def tro(self):
        return self.tro_bytes
        #  return self.packets_sent_link / float(self.packets_sent_stream_unmasked - self.sender_buffer_blocked) - 1

    @property
    def tro_bytes(self):
        bytes_sent_full = self.bytes_sent_link / (1 - (1 + drop_correlation +
                                                       drop_correlation ** 2 +
                                                       drop_correlation ** 3)
                                                  * self.drop_rate / 100)
        return (bytes_sent_full / self.bytes_sent_stream) - 1

    #  @property
    #  def ttd_max_jb(self):
    #      return self.trace_jb['ttd'].max()  # ns
    #
    #  @property
    #  def ttd_mean_jb(self):
    #      return self.trace_jb['ttd'].mean()
    #
    #  @property
    #  def ttd_std_jb(self):
    #      return self.trace_jb['ttd'].std()
    #
    #  @property
    #  def ttd_var_jb(self):
    #      return self.trace_jb['ttd'].var()

    #  def cdf(self):
    #      return cdf(self.trace['ttd'])

    #  def cdf_jb(self):
    #      return cdf(self.trace_jb['ttd'])

    #  def hist(self):
    #      return eval_hist(self.trace.ttd/1000000, min(int(self.ttd_max / 1000000 / 5), 50))

    def delay_over_time(self):
        '''returns the ttd over time t, starting with 0 at the first sending of a packet'''
        if args.pathfailure:
            self.trace_full['ttd'] = self.trace_full.now - self.trace_full.rtptime
            tmp = self.trace_full[['rtptime', 'ttd']]
        else:
            tmp = self.trace[['rtptime', 'ttd']]
        tmp['rtptime'] = (tmp['rtptime'] - self.start_time)/1000000000
        tmp['ttd'] = tmp['ttd']/1000000
        return tmp

    #  def delay_over_time_duplicates(self):
    #      '''returns the ttd over time t for duplicates, starting with 0 at the first sending of a packet'''
    #      tmp = self.trace_duplicates[['now', 'ttd']]
    #      tmp['now'] = (tmp['now'] - self.start_time)/1000000000
    #      tmp['ttd'] = tmp['ttd']/1000000
    #      return tmp

    #  def delay_over_time_jb(self):
    #      return (self.trace_jb['now'] - self.start_time), self.trace_jb['ttd']


class ResultsDir():
    def __init__(self, rpath):
        self.results_path = rpath
        with open(os.path.join(self.results_path, "experiments_config")) as exps_config_file:
            exps_config = exps_config_file.read()

            match = re.search(r'project_dir=(\S+)', exps_config)
            assert match is not None
            #  global project_dir
            self.project_dir = match[1]

            match = re.search(r'ip_sender=(\S+)', exps_config)
            assert match is not None
            #  global ip_sender
            self.ip_sender = match[1]

            match = re.search(r'ip_receiver=(\S+)', exps_config)
            assert match is not None
            #  global ip_receiver
            self.ip_receiver = match[1]

            match = re.search(r'phy1=(\S+)', exps_config)
            assert match is not None
            #  global phy1
            self.phy1 = match[1]

            match = re.search(r'phy2=(\S+)', exps_config)
            assert match is not None
            #  global phy2
            self.phy2 = match[1]

            #  match = re.search(r'variant=(\S+)', exps_config)
            #  global variants
            #  variants = match[1]

            #  match = re.search(r'delay_on_link=(\S+)', exps_config)
            #  global delay_on_link
            #  delay_on_link = match[1]

            match = re.search(r'deadline=(\d+)', exps_config)
            assert match is not None
            #  global deadline
            self.deadline = int(match[1])

            match = re.search(r'num_frames=(\d+)', exps_config)
            assert match is not None
            #  global num_frames
            self.num_frames = int(match[1])

            #  match = re.search(r'drop_rate=(\S+)', exps_config)
            #  global drop_rate
            #  drop_rate = match[1]

            match = re.search(r'padding=(\d+)', exps_config)
            assert match is not None
            #  global padding
            self.padding = int(match[1])

            match = re.search(r'runs=(\d+)', exps_config)
            assert match is not None
            #  global num_runs
            self.num_runs = int(match[1])
            #  num_runs = min(2, num_runs)  # TODO force only 2


def plot_delay_over_time(run):
    if args.save:
        save_file = os.path.join(args.lab, 'plots/ttd_time_%s_%03dms_%.1f_%0d.png' %
                                 (run.variant, run.delay, run.drop_rate, run.run_id))
        if os.path.isfile(save_file):
            print("skipped", save_file)
            return

    fig, ax = plt.subplots()
    ttd = run.delay_over_time()
    ttd.plot(x='rtptime', y='ttd', label=run.variant, legend=False,
             marker='.', linestyle='', markersize=1.,
             ax=ax, sharey=ax, sharex=ax)

    if args.pathfailure:
        plt.axvline(x=run.pf_time/1000000000, linestyle='--', color='gray', linewidth=1)
        plt.axvline(x=run.pf_time/1000000000 + pf_time_measure, linestyle='--', color='gray', linewidth=1)
    #  ttd_dupl = run.delay_over_time_duplicates()
    #  if len(ttd_dupl) > 0:
    #      ttd_dupl.plot(x='now', y='ttd', label=run.variant + ' dupl', legend=True,
    #                    marker='+', linestyle='', markersize=.5,
    #                    ax=ax, sharey=ax, sharex=ax)
    #  plt.title('TTD over time at delay {}ms, drop {}%'.format(run.delay, run.drop_rate))
    ax.set_ylabel('TTD [ms]')
    ax.set_xlabel('Time [s]')
    if args.save:
        fig.savefig(save_file, bbox_inches='tight')
        plt.close()
        print("saved", save_file)
    else:
        plt.show()
    plt.show()


def plot_hist(exp):
    fig, ax = plt.subplots()
    ax_hist = (exp.trace.ttd/1000000).plot.hist(bins=50, title='Histogram TTD', label='Histogram',
                                                alpha=.7, color='blue', grid=False,
                                                #  legend=True,
                                                ax=ax, sharey=ax, sharex=ax)

    ax_cdf = (exp.trace.ttd/1000000).plot.hist(bins=100, cumulative='True', normed=True,
                                               label='Cumulative', histtype='step',
                                               linestyle='-', color='blue', linewidth=1,
                                               #  legend=True, mark_right=False,
                                               secondary_y=True, ax=ax, sharey=ax, sharex=ax)
    plt.axvline(x=exp.deadline, linestyle='--', color='gray')
    ax.set_xlabel('TTD [ms]')
    ax.right_ax.set_ylabel('Cumulative')

    plt.show()


def plot_hist_over_drop(var, delay, drop=all):
    global all_exp
    plot_hist_multi([e for e in all_exp if e.variant == var and e.delay == delay
                     and (drop == all or e.drop_rate in drop) and e.variant is not 'dupl'],
                    label="str(e.drop_rate * (1 + drop_correlation))+'%'",
                    title='TTD for different PDR',
                    subtitle='Variant: %s, Link Delay: %dms' % (var, delay))


def plot_hist_over_delay(var, drop, delay=all):
    global all_exp
    plot_hist_multi([e for e in all_exp if e.variant == var and e.drop_rate == float(drop)
                     and (delay == all or e.delay in delay)],
                    label="str(e.delay)+'ms'", title='TTD for different Link Delay',
                    subtitle='Variant: {}, PDR: {:.1f}%'.format(var, drop * (1 + drop_correlation)))


def plot_hist_over_var(delay, drop):
    global all_exp
    plot_hist_multi([e for e in all_exp if e.drop_rate == float(drop) and e.delay == delay and e.variant is not 'dupl'],
                    label="str(e.variant)", title='TTD for different Duplication Variants',
                    subtitle='Link Delay: %dms, PDR: %.1f%%' % (delay, drop * (1 + drop_correlation)))


def plot_hist_multi(exps, label, title, subtitle=''):
    if len(exps) == 0:
        print("Empty experiments list")
        return

    if args.save:
        save_file = os.path.join(args.lab, 'plots/',
                                 title.lower().replace(" ", '_').replace('%', '') + '_' +
                                 subtitle.lower().replace(": ", '_').replace(", ", '_').replace(" ", '_').replace('%', '') + ".png")
        if os.path.isfile(save_file):
            print("skipped", save_file)
            return

    #  colors = iter(plot_colors)
    fig, ax = plt.subplots()
    for e in exps:
        #  c = next(colors)
        #  (e.trace.ttd/1000000).plot.hist(bins=min(150, int(e.ttd_max/1000000/10)), label=eval(label),
        #                                  alpha=.5, color=c, grid=False,
        #                                  legend=True, normed=True,
        #                                  ax=ax, sharey=ax, sharex=ax)

        ser = (e.trace.ttd/1000000).sort_values()
        # Now, before proceeding, append again the last (and largest) value. This step is important
        # especially for small sample sizes in order to get an unbiased CDF:
        ser[len(ser)] = ser.iloc[-1]
        # the sorted ttd values become the index (x) whereas 0..1 is applied to the y values
        ser_cdf = pd.Series(np.linspace(0., 1 - e.lost / e.packets_sent_stream,  # goes up to the number of packets arrived
                                        len(ser)), index=ser)
        ser_cdf.plot(label=eval(label), drawstyle='steps',
                     linewidth=1.5,
                     #  legend=True,  # mark_right=False, secondary_y=True,
                     ax=ax, sharey=ax, sharex=ax)

        #  (e.trace.ttd/1000000).plot.hist(bins=min(150, int(e.ttd_max/1000000/10)), cumulative='True', normed=True,
        #                                  label='Cumulative', histtype='step',
        #                                  linestyle='-', color=c, linewidth=1,
        #                                  legend=False, mark_right=False,
        #                                  secondary_y=True, ax=ax, sharey=ax, sharex=ax)

    plt.axvline(x=exps[0].deadline, linestyle='--', color='gray', linewidth=1)

    #  plt.suptitle(title, y=.96)
    #  plt.title(subtitle, fontsize=10)
    box = ax.get_position()
    #  ax.set_position([box.x0, box.y0, box.width * 0.85, box.height])
    ax.legend(loc='lower right')  # , bbox_to_anchor=(1., 1.01))
    ax.set_xlabel('TTD [ms]')
    ax.set_ylabel('Cumulative')
    plt.xlim((0, exps[0].deadline * 4 / 3))
    plt.ylim((0, 1))

    if args.save:
        fig.savefig(save_file, bbox_inches='tight')
        plt.close()
        print("saved", save_file)
    else:
        plt.show()


def plot_ddr_over_drop(delay):
    if args.save:
        save_file = os.path.join(args.lab, 'plots/ddr_over_drop_at_delay_%03dms.png' % delay)
        if os.path.isfile(save_file):
            print("skipped", save_file)
            return

    all_df_grouped = all_df[(all_df.variant != 'dupl') &
                            (all_df.delay == delay)].sort_values(
        by=['variant', 'drop']).groupby('variant', sort=False)
    if len(all_df_grouped) == 0:
        print('empty plot')
        return

    fig, ax = plt.subplots()
    for t, gr in all_df_grouped:
        gr['drop'] = gr['drop'] * (1 + drop_correlation)
        gr.plot(y='ddr', x='drop',
                linestyle='--', linewidth=.3,
                marker=plot_markers_variant[gr['variant'].iloc[0]],
                markersize=8, label=t,
                color=plot_colors_variant[gr['variant'].iloc[0]],
                ax=ax, sharex=ax, sharey=ax)  # , color=next(colors))

    #  plt.suptitle('{} for different Duplication Variants'.format("DDR" if not args.pathfailure else "PFI"), y=.96)
    #  plt.title('Link Delay: {}ms'.format(delay), fontsize=10)

    plt.xlim((0, 15))
    plt.ylim(top=1)
    box = ax.get_position()
    #  ax.set_position([box.x0, box.y0, box.width * 0.85, box.height])
    ax.legend(loc='lower left')  # , bbox_to_anchor=(1., 1.01))
    ax.set_ylabel('DDR' if not args.pathfailure else "PFI")
    ax.set_xlabel('Link PDR [%]')
    if args.save:
        fig.savefig(save_file, bbox_inches='tight')
        plt.close()
        print("saved", save_file)
    else:
        plt.show()


def plot_ddr_over_delay(drop):
    if args.save:
        save_file = os.path.join(args.lab, 'plots/ddr_over_delay_at_drop_%04.1f.png' % float(drop))
        if os.path.isfile(save_file):
            print("skipped", save_file)
            return

    all_df_grouped = all_df[(all_df.variant != 'dupl') &
                            (all_df['drop'] == float(drop))].sort_values(
        by=['variant', 'delay']).groupby('variant', sort=False)
    if len(all_df_grouped) == 0:
        print('empty plot')
        return

    fig, ax = plt.subplots()
    for t, gr in all_df_grouped:
        #  embed()
        gr.plot(y='ddr', x='delay',
                linestyle='--', linewidth=.3,
                marker=plot_markers_variant[gr['variant'].iloc[0]],
                markersize=8, label=t,
                color=plot_colors_variant[gr['variant'].iloc[0]],
                ax=ax, sharex=ax, sharey=ax)  # , color=next(colors))

    # plot 1/3 D as vertical grey line
    plt.axvline(x=(200 - gst_sched_delay * 1000) / 3,
                linestyle='--', color='gray', linewidth=1)

    #  plt.suptitle('{} for different Duplication Variants'.format("DDR" if not args.pathfailure else "PFI"), y=.96)
    #  plt.title('PDR: {:.1f}%'.format(float(drop * (1 + drop_correlation))), fontsize=10)

    plt.xlim((10, 110))
    plt.ylim(top=1)
    box = ax.get_position()
    #  ax.set_position([box.x0, box.y0, box.width * 0.85, box.height])
    #  ax.legend()
    ax.legend(loc='lower left')  # , bbox_to_anchor=(1., 1.01))
    ax.set_ylabel('DDR' if not args.pathfailure else "PFI")
    ax.set_xlabel('Link Delay [ms]')
    if args.save:
        fig.savefig(save_file, bbox_inches='tight')
        plt.close()
        print("saved", save_file)
    else:
        plt.show()


def plot_tro_over_drop(delay):
    if args.save:
        save_file = os.path.join(args.lab, 'plots/tro_over_drop_at_delay_%03dms.png' % delay)
        if os.path.isfile(save_file):
            print("skipped", save_file)
            return

    global all_df
    all_df_grouped = all_df[(all_df.variant != 'dpr_acc') &
                            (all_df.variant != 'dupl') &
                            (all_df.delay == delay)].sort_values(
                     by=['variant', 'drop']).groupby('variant', sort=False)
    if len(all_df_grouped) == 0:
        print('empty plot')
        return

    fig, ax = plt.subplots()
    for t, gr in all_df_grouped:
        gr['drop'] = gr['drop'] * (1 + drop_correlation)
        gr.plot(y='tro', x='drop',
                linestyle='--', linewidth=.3,
                marker=plot_markers_variant[gr['variant'].iloc[0]],
                markersize=8, label=t,
                color=plot_colors_variant[gr['variant'].iloc[0]],
                ax=ax, sharex=ax, sharey=ax)

    #  plt.suptitle('TO for different Duplication Variants', y=.96)
    #  plt.title('Link Delay: {}ms'.format(delay), fontsize=10)

    plt.xlim((0, 15))
    box = ax.get_position()
    #  ax.set_position([box.x0, box.y0, box.width * 0.85, box.height])
    ax.legend(loc='upper left')  # , bbox_to_anchor=(1., 1.01))
    ax.set_ylabel('TO')
    ax.set_xlabel('Link PDR [%]')

    if args.save:
        fig.savefig(save_file, bbox_inches='tight')
        plt.close()
        print("saved", save_file)
    else:
        plt.show()


def plot_tro_over_delay(drop):
    if args.save:
        save_file = os.path.join(args.lab, 'plots/tro_over_delay_at_drop_%04.1f.png' % float(drop))
        if os.path.isfile(save_file):
            print("skipped", save_file)
            return

    all_df_grouped = all_df[(all_df.variant != 'dpr_acc') &
                            (all_df.variant != 'dupl') &
                            (all_df['drop'] == float(drop))].sort_values(
        by=['variant', 'delay']).groupby('variant', sort=False)
    if len(all_df_grouped) == 0:
        print('empty plot')
        return

    fig, ax = plt.subplots()
    for t, gr in all_df_grouped:
        gr.plot(y='tro', x='delay',
                linestyle='--', linewidth=.3,
                marker=plot_markers_variant[gr['variant'].iloc[0]],
                markersize=8, label=t,
                #  legend=True,
                color=plot_colors_variant[gr['variant'].iloc[0]],
                ax=ax, sharex=ax, sharey=ax)

    # plot 1/3 D as vertical grey line
    plt.axvline(x=(200 - gst_sched_delay * 1000) / 3,
                linestyle='--', color='gray', linewidth=1)

    #  plt.suptitle('TO for different Duplication Variants', y=.96)
    #  plt.title('PDR: {:.1f}%'.format(float(drop * (1 + drop_correlation))), fontsize=10)

    plt.xlim((10, 110))
    box = ax.get_position()
    #  ax.set_position([box.x0, box.y0, box.width * 0.85, box.height])
    #  ax.legend()
    ax.legend(loc='upper left')  # , bbox_to_anchor=(1., 1.01))
    ax.set_ylabel('TO')
    ax.set_xlabel('Link Delay [ms]')
    if args.save:
        fig.savefig(save_file, bbox_inches='tight')
        plt.close()
        print("saved", save_file)
    else:
        plt.show()


def load_experiements(load_dirs, shelf=False):
    global all_exp
    global all_df
    all_exp = []
    for r in load_dirs:
        if 'plot' in r:
            print('plot dir')
            continue
        results_dir = ResultsDir(r)
        if shelf is False:
            shelf = shelve.open(os.path.join(r, "experiments"))
            shelf_opend = True
        else:
            shelf_opend = False

        for run_dir in glob.iglob(r + "/*/"):
            print('entering:', run_dir)
            exp_id = run_dir
            if args.deeprebuild is False and exp_id in shelf:
                print('loading experiment from shelf')
                exp = shelf[exp_id]
            else:
                exp = Experiment(run_dir, results_dir)
                shelf[exp_id] = exp

            if exp.num_runs > 0:
                all_exp.append(exp)
                all_df = all_df.append({'variant': exp.variant,
                                        'drop': exp.drop_rate,
                                        'delay': exp.delay,
                                        'hit': exp.deadline_hit,
                                        'miss':  exp.deadline_miss,
                                        'dupl': exp.duplicates,
                                        'lost': exp.lost,
                                        'ddr': exp.ddr,
                                        'runs': exp.num_runs,
                                        'frames': exp.num_frames,
                                        'deadline': exp.deadline,
                                        'to': exp.tro_bytes,
                                        'ttd_mean': exp.ttd_mean},
                                       ignore_index=True)
                print(exp)
            else:
                print('ignoring this empty experiment', run_dir)
            #  exp_id += 1
        if shelf_opend:
            shelf.close()


########
# MAIN #
########

parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument('--results', action='append', help='dir of restults from the experiment run')
parser.add_argument('--lab', help='dir of lab_restults (everything in there will be loaded..)')
parser.add_argument('--experiment', help='the single experiment to analyze')
parser.add_argument('--rebuild', action="store_true", help='recollect the experiment results')
parser.add_argument('--save', action="store_true", help='save plots as png')
parser.add_argument('--plotall', action="store_true", help='')
parser.add_argument('--pathfailure', action="store_true", help='')
parser.add_argument('--deeprebuild', action="store_true", help='deep recollect the experiment results')
args = parser.parse_args()


all_exp = []
all_df = pd.DataFrame(columns=['variant', 'drop', 'delay', 'hit', 'miss', 'dupl', 'lost', 'ddr',
                               'runs', 'frames', 'deadline', 'tro', 'ttd_mean'])


if args.lab:
    #  global all_df
    #  if args.rebuild is False shelf and 'all_df' in shelf:
    #      print('loading all experiments')
    #      all_exp = shelf['all']
    #  else:
    with shelve.open(os.path.join(args.lab, "experiments")) as shelf:
        print(args.rebuild, args.deeprebuild)
        if args.rebuild is True or args.deeprebuild is True or 'all_df' not in shelf:
            # rebuild
            load_experiements(glob.iglob(args.lab + "/*/"), shelf=shelf)
            shelf['all_df'] = all_df
            #  all_df = shelf['all_df']
        else:
            print("loading all experiements from the shelve")
            all_df = shelf['all_df']
            for r in glob.iglob(args.lab + "/*/*/"):
                e = shelf[r]
                if e.num_runs > 0:
                    all_exp.append(shelf[r])

        #  load_experiements(glob.iglob(args.lab + "/*/"), shelf=shelf)
    #  shelf['all'] = all_exp
else:
    load_experiements(args.results)

if args.save:
    if not args.lab:
        print("--save is only available with --lab")
        exit()
    if not os.path.exists(os.path.join(args.lab, 'plots')):
        os.makedirs(os.path.join(args.lab, 'plots'))


print("found", len(all_exp), 'experiments with altogether', sum(len(e.runs) for e in all_exp), 'useable runs')
print("Generating virtual dpr_acc experiments")
for e in [e for e in all_exp if e.variant == 'dpr']:
    e_new = copy(e)
    e_new.variant = 'dpr_acc'
    # accomodating
    e_new.accomodating_gst_delay = True
    all_exp.append(e_new)

    if args.rebuild:
        all_df = all_df.append({'variant': e_new.variant,
                                'drop': e_new.drop_rate,
                                'delay': e_new.delay,
                                'hit': e_new.deadline_hit,
                                'miss':  e_new.deadline_miss,
                                'dupl': e_new.duplicates,
                                'lost': e_new.lost,
                                'ddr': e_new.ddr,
                                'runs': e_new.num_runs,
                                'frames': e_new.num_frames,
                                'deadline': e_new.deadline,
                                'to': e_new.tro_bytes,
                                'ttd_mean': e_new.ttd_mean},
                                ignore_index=True)
        print(e_new)

if args.rebuild:
    with shelve.open(os.path.join(args.lab, "experiments")) as shelf:
        shelf['all_df'] = all_df

all_exp.sort(key=attrgetter('variant', 'delay', 'drop_rate'))

if args.plotall:
    if args.pathfailure:
        for e in all_exp:
            if e.variant == 'dpr_acc':
                continue
            for r in e.runs:
                print("last:", r.pf_last_seqnum,
                      "t_pf:", r.pf_time/1000000000.,
                      "t_pf:", r.pf_time/1000000000.,
                      "cutoff:", r.pf_last_seqnum + r.packets_sent_stream,
                      "trace:", len(r.trace), "/", r.packets_sent_stream)
                plot_delay_over_time(r)

        plot_ddr_over_delay(drop=0.)
        plot_ddr_over_delay(drop=10.)

        for i in (20, 40, 60, 80, 100, 120, 150):
            plot_hist_over_var(drop=0., delay=i)
            plot_hist_over_var(drop=10., delay=i)

    else:
        #  TTD traces
        for e in all_exp:
            if e.variant == 'dpr_acc':
                continue
            for r in e.runs:
                plot_delay_over_time(r)

        #  DDR
        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_ddr_over_delay(drop=i)

        for i in (5, 20, 40, 60, 80, 100, 120, 150, 200):
            plot_ddr_over_drop(delay=i)

        # TO
        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_tro_over_delay(drop=i)

        for i in (5, 20, 40, 60, 80, 100, 120, 150, 200):
            plot_tro_over_drop(delay=i)

        # Histograms over delay
        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_hist_over_delay(drop=i, var='dpr')

        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_hist_over_delay(drop=i, var='dupl')

        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_hist_over_delay(drop=i, var='udp')

        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_hist_over_delay(drop=i, var='udpdupl')

        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_hist_over_delay(drop=i, var='single')

        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_hist_over_delay(drop=i, var='cmt')

        # hist over drop
        for i in (5, 20, 40, 60, 80, 100, 120, 150, 200):
            plot_hist_over_drop(delay=i, var='cmt', drop=(0, .5, 2, 5, 7, 10))

        for i in (5, 20, 40, 60, 80, 100, 120, 150, 200):
            plot_hist_over_drop(delay=i, var='dupl', drop=(0, .5, 2, 5, 7, 10))

        for i in (5, 20, 40, 60, 80, 100, 120, 150, 200):
            plot_hist_over_drop(delay=i, var='single', drop=(0, .5, 2, 5, 7, 10))

        for i in (5, 20, 40, 60, 80, 100, 120, 150, 200):
            plot_hist_over_drop(delay=i, var='udp', drop=(0, .5, 2, 5, 7, 10))

        for i in (5, 20, 40, 60, 80, 100, 120, 150, 200):
            plot_hist_over_drop(delay=i, var='dpr', drop=(0, .5, 2, 5, 7, 10))

        for i in (20, 40, 60, 80, 100, 120, 150, 200):
            plot_hist_over_drop(delay=i, var='udpdupl', drop=(0, .5, 2, 5, 7, 10))

        # histograms on different variations
        for i in (0, 0.2, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            plot_hist_over_var(drop=i, delay=20)
            plot_hist_over_var(drop=i, delay=40)
            plot_hist_over_var(drop=i, delay=60)
            plot_hist_over_var(drop=i, delay=80)
            plot_hist_over_var(drop=i, delay=100)

else:
    # plot_hist_over_drop(var='dpr', delay=40)
    #  for r in all_exp[0].runs:
    #      plot_delay_over_time(r)
    if args.pathfailure is True:
        for e in all_exp:
            # print latex table
            print("{variant:20}& {delay:3}ms & {num_runs:2} & {ddr:6.4f}\\\\ \midrule".format(
                #  "{ttd_mean:3.0f}ms & {ttd_q95:3.0f}ms & {tro:5.2f} & {tro_exp:5.2f} {sent: 8d} | {lost: 5d} ({loss_rate:4.1f})% | {dupl: 4d} ({dupl_rate:4.1f})% ".format(
                    variant="\\texttt{" + e.variant.replace('_', '\\_') + "}",
                    delay=e.delay,
                    drop_rate=e.drop_rate * (1 + drop_correlation),
                    exp_ddr=e.ddr_expected,
                    ddr_error=e.ddr - e.ddr_expected,
                    num_runs=e.num_runs,
                    ddr=e.ddr,
                    ddr_mean=st.mean(r.ddr for r in e.runs),
                    ddr_std=st.stdev(r.ddr for r in e.runs) if e.num_runs > 1 else 0,
                    ttd_mean=e.ttd_median / 1000000,
                    ttd_q95=e.ttd_quantile(0.99) / 1000000,
                    tro=e.tro_bytes,
                    tro_exp=e.tro_expected,
                    sent=e.packets_sent_stream,
                    lost=e.lost,
                    loss_rate=(e.lost / e.packets_sent_stream_unmasked) * 100 if e.lost != 0 else 0,
                    dupl=e.duplicates,
                    dupl_rate=(e.duplicates_unmasked / e.packets_sent_stream_unmasked) * 100 if e.duplicates_unmasked != 0 else 0
                ))
    else:
        for e in all_exp:
            # print latex table
            print("{variant:20}& {delay:3}ms & {drop_rate:4.1f}\\% & {num_runs:2} & "
                "{exp_ddr:6.3f} & {ddr:6.4f} & {ddr_std:7.5f} & {ddr_error:8.5f} & "
                "{ttd_mean:3.0f}ms & {ttd_q95:3.0f}ms & {tro_exp:5.2f} & {tro:5.2f} \\\\ \midrule".format(
                #  "{ttd_mean:3.0f}ms & {ttd_q95:3.0f}ms & {tro:5.2f} & {tro_exp:5.2f} {sent: 8d} | {lost: 5d} ({loss_rate:4.1f})% | {dupl: 4d} ({dupl_rate:4.1f})% ".format(
                    variant="\\texttt{" + e.variant.replace('_', '\\_') + "}",
                    delay=e.delay,
                    drop_rate=e.drop_rate * (1 + drop_correlation),
                    exp_ddr=e.ddr_expected,
                    ddr_error=e.ddr - e.ddr_expected,
                    num_runs=e.num_runs,
                    ddr=e.ddr,
                    ddr_mean=st.mean(r.ddr for r in e.runs),
                    ddr_std=st.stdev(r.ddr for r in e.runs) if e.num_runs > 1 else 0,
                    ttd_mean=e.ttd_median / 1000000,
                    ttd_q95=e.ttd_quantile(0.99) / 1000000,
                    tro=e.tro_bytes,
                    tro_exp=e.tro_expected,
                    sent=e.packets_sent_stream,
                    lost=e.lost,
                    loss_rate=(e.lost / e.packets_sent_stream_unmasked) * 100 if e.lost != 0 else 0,
                    dupl=e.duplicates,
                    dupl_rate=(e.duplicates_unmasked / e.packets_sent_stream_unmasked) * 100 if e.duplicates_unmasked != 0 else 0
                ))

    embed()
exit()
