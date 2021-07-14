/** @file tm_data_sndr.cc */
#include "dasio/tm_data_sndr.h"
#include "dasio/loop.h"
#include "nl.h"

namespace DAS_IO {
  
TM_data_sndr::TM_data_sndr(const char *iname, const char *hostname,
      const char *datum, const void *data, uint16_t size)
    : Client(iname, 10, hostname, "tm_gen", 0),
      data(data),
      data_len(size),
      gflag_no(0),
      overrun_obuf(false) {
  snprintf(sub_service, subsvc_len, "data/%s", datum);
  set_subservice(sub_service);
}

bool TM_data_sndr::app_input() {
  // msg(0, "%s: TM_data_sndr::app_input(%d)", iname, nc);
  report_ok(nc);
  return Send();
}

bool TM_data_sndr::app_connected() {
  // msg(0, "TM_data_sndr::app_connected()");
  return Send();
}

bool TM_data_sndr::Send() {
  if (obuf_empty()) {
    if (iwrite((const char *)data, data_len))
      return true;
    if (ELoop) ELoop->set_gflag(gflag_no);
  } else if (!overrun_obuf) {
    msg(MSG_ERROR, "%s: TM Send() skipped", iname);
    overrun_obuf = true;
  }
  return false;
}

}
