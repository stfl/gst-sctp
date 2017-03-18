
void
usrsctp_debug_printf(const char *format, ...)
{
   va_list ap;
   gchar *out;

   va_start(ap, format);
   out = gst_info_strdup_vprintf(format, ap);
   /* vprintf(format, ap); */
   va_end(ap);

   out[strcspn(out, "\n")] = '\0';
   GST_DEBUG("%s", out);
   g_free(out);
}
