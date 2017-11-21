import argparse
import numpy as np
#  import numpy.lib.recfunctions as rcfuncs
import pandas as pd

import shelve
import sys
from os import path
import glob
import re

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
matplotlib.style.use('ggplot')

#  from scipy.interpolate import UnivariateSpline
#  import math

# config
fps = 24
init_offset = 12  # frames = 0.5 s
num_packets_per_frame = 11
cutoff_init = int(init_offset * num_packets_per_frame)
cutoff_end = num_packets_per_frame  # 1 frame

plot_colors = ['maroon', 'red', 'olive', 'yellow', 'green', 'lime', 'teal', 'orange', 'aqua', 'navy',
               'blue', 'purple', 'fuchsia', 'maroon', 'green']


class Experiment:
    '''an Experiment is a set of runs with the same characteristics, like delay, packet drop rate
    and the variant used set.'''

    def __init__(self, run_path, results_dir):
        self.run_path = run_path
        self.results_dir = results_dir
        self.runs = []
        self.num_runs = results_dir.num_runs
        self.__collect_runs()

        if self.num_runs > 0:
            self.trace = pd.concat([r.trace for r in self.runs])

            assert len(self.trace) + self.lost == self.packets_sent
        #  self.all_ttd_jb = np.concatenate([r.trace_out['ttd'] for r in self.runs])
        #  assert len(self.all_ttd_jb) + self.lost == self.packets_sent

    def __collect_runs(self):
        with open(path.join(self.run_path, "run_config")) as run_config_file:
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
                self.runs.append(Run(self.run_path, run_id, self.results_dir, self.variant))
            except (InvalidValue, ReceiverKilled):
                print("Ignoring this run")
                self.num_runs -= 1
                continue
        assert len(self.runs) == self.num_runs

    @property
    def deadline(self):
        return self.results_dir.deadline

    #  def cdf_in(self, variant):
    @property
    def ddr(self):
        return float(self.deadline_hit / self.packets_sent)

    @property
    def packets_sent(self):
        return sum(r.packets_sent for r in self.runs)

    @property
    def packets_sent_unmasked(self):
        return sum(r.packets_sent_unmasked for r in self.runs)

    @property
    def deadline_hit(self):
        #  return sum(r.deadline_hit for r in self.runs)
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
    def ttd_std(self):
        return self.trace.ttd.std()

    @property
    def ttd_var(self):
        return self.trace.ttd.var()

    @property
    def sender_tx(self):
        return [sum(r.sender_tx[0] for r in self.runs), sum(r.sender_tx[1] for r in self.runs)]

    def hist(self):
        return eval_hist(self.trace.ttd/1000000, min(int(self.ttd_max / 1000000 / 5), 50))

    def ttd_quantile(self, q):
        return self.trace.ttd.quantile(q, interpolation='lower')

    @property
    def tro(self):
        return (self.duplicates_unmasked + self.packets_sent_unmasked) / float(self.packets_sent_unmasked) - 1

    @property
    def sender_buffer_blocked(self):
        return sum(r.sender_buffer_blocked for r in self.runs)

    @property
    def num_frames(self):
        return self.results_dir.num_frames

    def __str__(self):
        q1, q2 = self.ttd_quantile([.8, .95]) / 1000000
        exp_str = ("Variant:{variant} delay:{delay}ms droprate:{drop:.1f}% D:{deadline}ms frames:{frames} {runs}runs\n"
                   "sent:{sent:5d} hit:{hit:5d} miss:{miss:4d} lost:{lost:4d} ({ml:4d}) DDR:{ddr:.2%}\n"
                   "TTD mean:{mean:6.2f} std:{std:6.2f} max:{max:6.2f} q80:{q1:6.2f} q95:{q2:6.2f}\n"
                   "dupl:{dupl:5d} TRO:{tro:.2%} snd blk:{send_block} tr: {s1:.0f}/{s2:.0f}kB\n"
                   .format(delay=self.delay,
                           deadline=self.deadline,
                           drop=self.drop_rate,
                           variant=self.variant,
                           runs=self.num_runs,
                           frames=self.results_dir.num_frames,
                           ddr=self.ddr,
                           sent=self.packets_sent,
                           hit=self.deadline_hit,
                           miss=self.deadline_miss,
                           lost=self.lost,
                           ml=self.deadline_miss + self.lost,
                           dupl=self.duplicates_unmasked,
                           tro=self.tro,
                           mean=self.ttd_mean/1000000,
                           std=self.ttd_std/1000000,
                           max=self.ttd_max/1000000,
                           q1=q1, q2=q2,
                           send_block=self.sender_buffer_blocked,
                           s1=self.sender_tx[0]/1000,
                           s2=self.sender_tx[1]/1000))
        #  ddr_str = ("sent:{sent:5d} hit:{hit:5d} miss:{miss:4d} lost:{lost:4d} ({ml:4d}) DDR:{ddr:.2%}\n"
                   #  .format(ddr=self.ddr, sent=self.packets_sent, hit=self.deadline_hit,
                   #          miss=self.deadline_miss + self.lost, dupl=self.duplicates_unmasked,
                   #          tro=self.tro))
        #  ttd_str = ("TTD mean:{mean:6.2f} std:{std:6.2f} max:{max:6.2f} q80:{q1:6.2f} q95:{q2:6.2f}\n"
                   #  .format(mean=self.ttd_mean/1000000,
                   #          std=self.ttd_std/1000000,
                   #          max=self.ttd_max/1000000,
                   #          q1=q1, q2=q2))
        #  other_str = ('dupl:{dupl:5d} TRO:{tro:.2%} snd blk:{send_block} tr: {s1:.0f}/{s2:.0f}kB\n'
                     #  .format(send_block=self.sender_buffer_blocked,
                     #          s1=self.sender_tx[0]/1000, s2=self.sender_tx[1]/1000))

        return exp_str #+ ddr_str + ttd_str + other_str


class Run():
    '''A Run is a single run with a defined number of frames transfered through gstreamer'''
    def __init__(self, run_path, run_id, results_dir, variant):
        self.run_path = run_path
        self.run_id = run_id
        self.variant = variant
        self.results_dir = results_dir
        self.duplicates_unmasked = 0
        self.packets_sent_unmasked = self.results_dir.num_frames * num_packets_per_frame
        self.packets_sent = self.packets_sent_unmasked - cutoff_init - cutoff_end
        self.receiver_killed = path.isfile(path.join(self.run_path, 'receiver_killed_' + str(self.run_id)))
        if self.receiver_killed:
            print('receiver killed')
            raise ReceiverKilled

        file_experiment_trace_in = path.join(self.run_path, 'experiment_trace_in_' + str(self.run_id) + '.csv')
        if not path.isfile(file_experiment_trace_in):
            print("trace not found")
            raise InvalidValue

        #  file_experiment_trace_out = path.join(self.run_path, 'experiment_trace_out_' + str(self.run_id) + '.csv')
        #  self.trace = np.genfromtxt(file_experiment_trace_in, names=True, delimiter=';', dtype=np.int64)
        #  self.trace_jb = np.genfromtxt(self.file_experiment_trace_out, names=True, delimiter=';', dtype=int)
        self.trace = pd.read_csv(file_experiment_trace_in, delimiter=';')
        if len(self.trace) < cutoff_init:
            print("trace too short")
            raise InvalidValue
        self.trace_all = self.trace

        self.trace = self.trace.drop_duplicates('seqnum')  # keeps the first, by index! (which is the lower "now")
        self.duplicates_unmasked = len(self.trace_all) - len(self.trace)

        # cut off init phase and end phase
        self.trace = self.trace[(self.trace.seqnum >= cutoff_init) & (self.trace.seqnum < (self.packets_sent_unmasked - cutoff_end))]
        self.trace['ttd'] = self.trace.now - self.trace.rtptime
        self.start_time = min(self.trace['rtptime'])
        #  self.trace_jb = self.trace_jb[cutoff_mask_jb]

        if min(self.trace.ttd) <= 0:
            print("negative ttd in ", file_experiment_trace_in)
            #  fig, ax = plt.subplots()
            #  df = pd.DataFrame(self.trace, columns=['rtptime', 'now']) / 1000000
            #  df.plot(sharey=True, ax=ax)
            #  (self.trace.ttd / 1000000).plot(secondary_y=True, label='TTD', legend=True, mark_right=False)
            #  ax.set_xlabel('Sequencenumer')
            #  ax.set_ylabel('Timestamp [ms]')
            #  ax.right_ax.set_ylabel('TTD [ms]')
            #  plt.show()

            #  raise InvalidValue
        #  assert min(self.trace.ttd) > 0

        # calc DDR
        self.deadline_hit = len(self.trace.query('now < deadline'))
        self.lost = self.packets_sent - len(self.trace)
        self.deadline_miss = len(self.trace) - self.deadline_hit
        self.ddr = float(self.deadline_hit / self.packets_sent)

        with open(path.join(self.run_path, "out_sender_" + str(self.run_id))) as out_sender_file:
            out_sender = out_sender_file.read()
            self.sender_buffer_blocked = len(re.findall(r"usrsctp_sendv failed:", out_sender))
            if self.sender_buffer_blocked > self.packets_sent_unmasked * 0.2:
                # ignore if more than 20% are sender buffer blocked
                print('too much sender buffer blocking')
                raise InvalidValue

        self.__calc_transfered_bytes()
        if self.variant in ('dpr', 'cmt', 'dupl', 'udpdupl'):
            if (min(self.sender_tx[0], self.sender_tx[1]) * 5 < max(self.sender_tx[0], self.sender_tx[1])):
                print('multihoming seams not to be used. phy1:{:.0f}kB phy2:{:.0f}kB  diff: {:.0f}kB'
                      .format(self.sender_tx[0]/1000, self.sender_tx[1]/1000, abs(self.sender_tx[0] - self.sender_tx[1])/1000))
                raise InvalidValue

        if (self.variant not in ['udp', 'udpdupl']):
            self.__read_usrsctp_stats()

    def __read_usrsctp_stats(self):
        with open(path.join(self.run_path, "usrsctp_stats_receiver_" + str(self.run_id))) as usrsctp_stats_receiver_file:
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

        with open(path.join(self.run_path, "usrsctp_stats_sender_" + str(self.run_id))) as usrsctp_stats_sender_file:
            usrsctp_stats_sender = usrsctp_stats_sender_file.read()

            match = re.search(r"sent_packets=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_sent_packets = int(match[1]) - 1  # the first one is a dummy

            match = re.search(r"recv_packets=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_recv_packets = int(match[1])

            match = re.search(r"send_data=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_recv_data = int(match[1]) - 1  # the first one is a dummy

            match = re.search(r"send_sacks=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_send_sacks = int(match[1])

            match = re.search(r"recv_sacks=(\d+)", usrsctp_stats_sender)
            assert match is not None
            self.sender_recv_sacks = int(match[1])

            #  send_retrans_data=4
            #  send_fast_retrans=4
            #  timer_dpr_fired=189
            #  dpr_avg_delay_timer=45552
            #  dpr_flagged=0
            #  abandoned_sent=0
            #  abandoned_unsent=0

    def __calc_transfered_bytes(self):
        with open(path.join(self.run_path, "sender_transfered_bytes_" + str(self.run_id))) as sender_tr_file:
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

        with open(path.join(self.run_path, "receiver_transfered_bytes_" + str(self.run_id))) as receiver_tr_file:
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

        #  pre = open(path.join(self.run_path, 'receiver_' + self.results_dir.phy1 + '_rx_bytes_pre_' + str(self.run_id)))
        #  post = open(path.join(self.run_path, 'receiver_' + self.results_dir.phy1 + '_rx_bytes_post_' + str(self.run_id)))
        #  self.receiver_rx = [int(post.read()) - int(pre.read())]
        #  pre.close()
        #  post.close()
        #
        #  pre = open(path.join(self.run_path, 'receiver_' + self.results_dir.phy2 + '_rx_bytes_pre_' + str(self.run_id)))
        #  post = open(path.join(self.run_path, 'receiver_' + self.results_dir.phy2 + '_rx_bytes_post_' + str(self.run_id)))
        #  self.receiver_rx.append(int(post.read()) - int(pre.read()))
        #  pre.close()
        #  post.close()
        #
        #  # TODO for tx bytes
        #
        #  pre = open(path.join(self.run_path, 'sender_' + self.results_dir.phy1 + '_tx_bytes_pre_' + str(self.run_id)))
        #  post = open(path.join(self.run_path, 'sender_' + self.results_dir.phy1 + '_tx_bytes_post_' + str(self.run_id)))
        #  embed()
        #  self.sender_tx = [int(re.match(r'(\d*)', post.read())[1]) - int(re.match(r'\d*', pre.read()))]
        #  pre.close()
        #  post.close()
        #
        #  pre = open(path.join(self.run_path, 'sender_' + self.results_dir.phy2 + '_tx_bytes_pre_' + str(self.run_id)))
        #  post = open(path.join(self.run_path, 'sender_' + self.results_dir.phy2 + '_tx_bytes_post_' + str(self.run_id)))
        #  self.sender_tx.append(int(post.read()) - int(pre.read()))
        #  pre.close()
        #  post.close()
        #
        #  print(self.sender_tx, self.receiver_rx)

        # TODO for Sender

    #  def __gen_duplicates_mask(self):
    #      '''generate the mask that assigns stores False on the possition of all sequence numbers'''
    #      duplicates_mask = [True]  # the first one cannot be a dupl
    #      for i in range(1, len(self.trace)):
    #          if self.trace['seqnum'][i] in self.trace['seqnum'][:i-1]:
    #              duplicates_mask.append(False)
    #              self.duplicates_unmasked += 1
    #          else:
    #              duplicates_mask.append(True)
    #      return duplicates_mask

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
        return (self.duplicates_unmasked + self.packets_sent_unmasked) / float(self.packets_sent_unmasked) - 1

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

    def cdf(self):
        return cdf(self.trace['ttd'])

    #  def cdf_jb(self):
    #      return cdf(self.trace_jb['ttd'])

    def hist(self):
        return eval_hist(self.trace.ttd/1000000, min(int(self.ttd_max / 1000000 / 5), 50))

    def delay_over_time(self):
        '''returns the ttd over time t, starting with 0 at the first sending of a packet'''
        return (self.trace['now'] - self.start_time), self.trace['ttd']

    #  def delay_over_time_jb(self):
    #      return (self.trace_jb['now'] - self.start_time), self.trace_jb['ttd']


#  def eval_hist(trace, bins):
#      factor = pd.cut(trace, bins)
#      factor = pd.value_counts(factor) / len(trace)
#      #  import pdb; pdb.set_trace()
#      #  factor.index = [(i.right + i.left)/2 for i in factor.index]
#      return factor


#  def cdf(array):
#      # https://stackoverflow.com/questions/10640759/how-to-get-the-cumulative-distribution-function-with-numpy
#      x = np.sort(array)
#      f = np.array(range(len(array)))/float(len(array))
#      return x, f

class ResultsDir():
    def __init__(self, rpath):
        self.results_path = rpath
        with open(path.join(self.results_path, "experiments_config")) as exps_config_file:
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


#  def plot_delay_over_time(run):
#      t, ttd = exp.runs[0].delay_over_time()
#      plt.plot(t / 1000000000, ttd / 1000000, 'r.')


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
    plot_hist_multi([e for e in all_exp if e.variant == var and e.delay == delay and e.drop_rate in drop],
                    label="str(e.drop_rate)+'%'", title='TTD at different Packet Drop Rates')


def plot_hist_over_delay(var, drop):
    global all_exp
    plot_hist_multi([e for e in all_exp if e.variant == var and e.drop_rate == float(drop)],
                    label="str(e.delay)+'ms'", title='TTD at different Link Delays')


def plot_hist_over_var(delay, drop):
    global all_exp
    plot_hist_multi([e for e in all_exp if e.drop_rate == float(drop) and e.delay == delay],
                    label="str(e.variant)", title='TTD at different Duplication Variants')


def plot_hist_multi(exps, label, title):
    if len(exps) == 0:
        print("Empty experiments list")
        return

    colors = iter(plot_colors)

    fig, ax = plt.subplots()
    for e in exps:
        c = next(colors)
        (e.trace.ttd/1000000).plot.hist(bins=min(150, int(e.ttd_max/1000000/10)), label=eval(label),
                                        alpha=.5, color=c, grid=False,
                                        legend=True, normed=True,
                                        ax=ax, sharey=ax, sharex=ax)

        ser = (e.trace.ttd/1000000).sort_values()
        # Now, before proceeding, append again the last (and largest) value. This step is important especially for small sample sizes in order to get an unbiased CDF:
        ser[len(ser)] = ser.iloc[-1]
        # the sorted ttd values become the index (x) whereas 0..1 is applied to the y values
        ser_cdf = pd.Series(np.linspace(0., 1., len(ser)), index=ser)
        #  exps[0].deadline * 4 / 3
        #  ser_cdf[600] = 1.
        # TODO continue plot with 1 afterwards
        ser_cdf.plot(label='Cumulative', drawstyle='steps', color=c, linewidth=1,
                     legend=False, mark_right=False,
                     secondary_y=True, ax=ax, sharey=ax, sharex=ax)

        #  (e.trace.ttd/1000000).plot.hist(bins=min(150, int(e.ttd_max/1000000/10)), cumulative='True', normed=True,
        #                                  label='Cumulative', histtype='step',
        #                                  linestyle='-', color=c, linewidth=1,
        #                                  legend=False, mark_right=False,
        #                                  secondary_y=True, ax=ax, sharey=ax, sharex=ax)

    plt.axvline(x=exps[0].deadline, linestyle='--', color='gray')

    plt.title(title)
    ax.set_xlabel('TTD [ms]')
    #  ax.right_ax.set_ylabel('Cumulative')
    plt.xlim((0, exps[0].deadline * 4 / 3))

    if args.save:
        fig.savefig(path.join('./plots/', title.lower().replace(" ", '_') + ".png"))
    else:
        plt.show()


def plot_ddr_over_drop(delay):
    colors = iter(plot_colors)
    fig, ax = plt.subplots()
    global all_df

    all_df_grouped = all_df[all_df.delay == delay].sort_values('drop').groupby('variant', sort=False)
    for t, gr in all_df_grouped:
        gr.plot(y='ddr', x='drop',
                label=t.capitalize() if t in ['dupl', 'single'] else t.upper(),
                ax=ax, sharex=ax, sharey=ax, color=next(colors))
    plt.title('DDR at different Duplication Variants for Delay {}ms'.format(delay))
    # TODO add as text
    ax.set_ylabel('DDR [%]')
    ax.set_xlabel('Packet Drop Rate on the link [%]')
    if args.save:
        fig.savefig('./plots/ddr_over_drop_delay_%d.png' % delay)
    else:
        plt.show()


def plot_ddr_over_delay(drop):
    colors = iter(['blue', 'red', 'green', 'orange', 'darkviolet'])
    # TODO sort colors
    #  colors = iter(plot_colors)
    fig, ax = plt.subplots()

    all_df_grouped = all_df[all_df['drop'] == float(drop)].sort_values('delay').groupby('variant', sort=False)
    for t, gr in all_df_grouped:
        gr.plot(y='ddr', x='delay',
                label=t.capitalize() if t in ['dupl', 'single'] else t.upper(),
                ax=ax, sharex=ax, sharey=ax, color=next(colors))
    plt.title('DDR at different Duplication Variants for Drop Rate {:.1f}%'.format(float(drop)))

    ax.set_ylabel('DDR [%]')
    ax.set_xlabel('Delay on the link [%]')
    if args.save:
        fig.savefig('./plots/ddr_over_delay_drop_%.1f.png' % float(drop))
    else:
        plt.show()


def load_experiements(load_dirs, shelf=False):
    global all_exp
    global all_df
    all_exp = []
    for r in load_dirs:
        results_dir = ResultsDir(r)
        if shelf is False:
            shelf = shelve.open(path.join(r, "experiments"))
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
                                        'tro': exp.tro,
                                        'ttd_mean': exp.ttd_mean}, ignore_index=True)
                #  print(exp)
            else:
                print('ignoring this empty experiment', run_dir, '\n')
            #  exp_id += 1
        if shelf_opend:
            self.close()


########
# MAIN #
########

parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument('--results', action='append', help='dir of restults from the experiment run')
parser.add_argument('--lab', help='dir of lab_restults (everything in there will be loaded..)')
parser.add_argument('--experiment', help='the single experiment to analyze')
parser.add_argument('--rebuild', action="store_true", help='recollect the experiment results')
parser.add_argument('--save', action="store_true", help='save plots as png')
parser.add_argument('--deeprebuild', action="store_true", help='deep recollect the experiment results')
args = parser.parse_args()


all_exp = []
all_df = pd.DataFrame(columns=['variant', 'drop', 'delay', 'hit', 'miss', 'dupl', 'lost', 'ddr',
                               'runs', 'frames', 'deadline', 'tro', 'ttd_mean'])

#  import ipdb; ipdb.set_trace()
if args.lab:
    #  global all_df
    #  if args.rebuild is False shelf and 'all_df' in shelf:
    #      print('loading all experiments')
    #      all_exp = shelf['all']
    #  else:
    with shelve.open(path.join(args.lab, "experiments")) as shelf:
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

print("found", len(all_df), 'experiments with altogether', sum(all_df.runs), 'runs')

#  plot_hist_over_delay('udp', drop=5)
plot_hist_over_drop('udp', delay=20, drop=(.5, 2, 5, 10))
embed()
#  plot_hist_over_drop('dpr', 60)
#  plot_hist_over_drop('single', 60)

#  plot_hist_over_drop('udp', 60)
#  plot_hist_over_drop('dupl', 60)
#  plot_hist_over_var(60, 10)
#  plot_ddr_over_drop(delay=40)
#  plot_ddr_over_delay(drop=8)

#  plot_hist_over_drop('udp', delay=30)
#  plot_hist_over_drop('udpdupl', delay=40)
#  plot_hist_over_var(drop=5, delay=40)

#  embed()

exit()
