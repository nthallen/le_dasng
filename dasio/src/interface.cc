/**
 * \file interface.cc
 */
#include <errno.h>
#include <string>
#include <unistd.h>
#include <string.h>
#include "dasio.h"
#include "nl_assert.h"

/**
 * I am assuming here that this level is low enough that I don't
 * need to have two separate invocations, once for deferred
 * initialization.
 * I am currently choosing to avoid providing an output buffer
 * here.
 * @param name An identifier for the interface to be used when
 * reporting statistics.
 * @param bufsz The size of the input buffer
 */
DAS_IO_Interface::DAS_IO_Interface(const char *name, int bufsz) {
  iname = name;
  nc = cp = 0;
  bufsize = bufsz;
  buf = bufsz > 0 ? (unsigned char *)new_memory(bufsz) : 0;
  fd = -1;
  flags = 0;
  Loop = 0;
  n_fills = n_empties = n_eagain = n_eintr = 0;
  obuf = 0;
  onc = ocp = 0;
  n_errors = 0;
  n_suppressed = 0;
  total_errors = 0;
  total_suppressed = 0;
}

DAS_IO_Interface::~DAS_IO_Interface() {
  // clean up children
}

/**
 * Virtual method to allow an interface to bid on the select() timeout
 * along with the Loop. The minimum timeout value is used.
 * @return a Timeout * indicating the requested timeout value or NULL.
 */
Timeout *DAS_IO_Interface::GetTimeout() {
  return &TO;
}

/**
 * Additional commentary on ProcessData.
 * Much more work to do here!!!
 */
bool DAS_IO_Interface::ProcessData(int flag) {
  return true;
}

/**
 * Sets up a write of nc bytes from the buffer pointed to by str.
 * If the write cannot be accomplished immediately, the information
 * is saved and handled transparently. The caller is
 * responsible for allocating the output buffer(s) and ensuring
 * they are not overrun.
 * The caller can check whether ocp >= onc to determine whther the
 * write has completed.
 * @param str Pointer to the output buffer
 * @param nc The total number of bytes in the output buffer
 * @param cp The starting offset within the output buffer
 * @return true if a fatal error occurs
 */
bool DAS_IO_Interface::iwrite(const char *str, unsigned int nc, unsigned int cp) {
  nl_assert(fd >= 0);
  obuf = (unsigned char *)str;
  onc = nc;
  ocp = cp;
  return iwrite_check();
}

bool DAS_IO_Interface::iwrite_check() {
  if (!obuf_empty()) {
    int nb = onc-ocp;
    int ntr = write(fd, obuf, nb);
    if (ntr < 0 && errno != EAGAIN) {
      flags &= ~Fl_Write;
      return(iwrite_error(errno));
    }
    if (ntr > 0) {
      ocp += ntr;
      iwritten(ntr);
    }
  }
  flags = obuf_empty() ?
    flags & ~Fl_Write :
    flags | Fl_Write;
  return false;
}

bool DAS_IO_Interface::iwrite(const std::string &s) {
  return iwrite(s.c_str(), s.length());
}

bool DAS_IO_Interface::iwrite(const char *str) {
  return iwrite(str, strlen(str));
}

/**
 * The default implementation does nothing.
 */
void DAS_IO_Interface::iwritten(int nb) {}

/**
 * The default implementation returns true.
 */
bool DAS_IO_Interface::iwrite_error(int my_errno) {
  nl_error(2, "%s: write error %d: %s", iname, my_errno, strerror(my_errno));
  return true;
}

/**
 * The default function returns true.
 */
bool DAS_IO_Interface::read_error(int my_errno) {
  nl_error(2, "%s: read error %d: %s", iname, my_errno, strerror(my_errno));
  return true;
}

/**
 * The default reports unexpected input and returns false;
 */
bool DAS_IO_Interface::protocol_input() {
  if (fillbuf()) return true;
  cp = 0;
  if (nc > 0)
    report_err("Unexpected input");
  return false;
}

/**
 * The default does nothing and returns false.
 */
bool DAS_IO_Interface::protocol_timeout() {
  return false;
}

/**
 * The default does nothing and returns false.
 */
bool DAS_IO_Interface::protocol_except(int flag) {
  return false;
}

bool DAS_IO_Interface::fillbuf(int N) {
  int i;
  if (!buf) nl_error(4, "Ser_Sel::fillbuf with no buffer");
  if (N > bufsize)
    nl_error(4, "Ser_Sel::fillbuf(N) N > bufsize: %d > %d",
      N, bufsize);
  if (nc > N-1) return 0;
  ++n_fills;
  i = read( fd, &buf[nc], N - 1 - nc );
  if ( i < 0 ) {
    if ( errno == EAGAIN ) {
      ++n_eagain;
    } else if (errno == EINTR) {
      ++n_eintr;
    } else {
      nl_error( 2, "Error %d on read from serial port", errno );
      return 1;
    }
    return 0;
  }
  nc += i;
  buf[nc] = '\0';
  return 0;
}

void DAS_IO_Interface::consume(int nchars) {
  if ( nchars > 0 ) {
    ++n_empties;
    if ( nchars < nc ) {
      int nb = nc - nchars;
      memmove(&buf[0], &buf[nchars], nb+1);
      nc = nb;
    } else {
      nc = 0;
    }
    cp = 0;
  }
}

void DAS_IO_Interface::report_err( const char *msg, ... ) {
  ++total_errors;
  if ( n_errors < QERR_THRESHOLD )
    ++n_errors;
  if ( n_suppressed == 0 && n_errors < QERR_THRESHOLD ) {
    va_list args;

    va_start(args, fmt);
    msgv( 2, fmt, args );
    va_end(args);
    if (nc)
      msg(2, "%s: Input was: '%s'", iname, ascii_escape((char*)buf, nc) );
  } else {
    if ( !n_suppressed )
      msg(2, "%s: Error threshold reached: suppressing errors", iname);
    ++n_suppressed;
    ++total_suppressed;
  }
}

#ifdef IMPLEMENTED
/**
 * Signals that a successful protocol transfer occurred,
 * reducing the qualified error count, potentially reenabling
 * logging of errors.
 */
void DAS_IO_Interface::report_ok();
/**
 * Parsing routines useful for parsing ASCII input data.
 * All of these operate on buf with the current offset
 * at cp and the total number of characters being nc.
 * These all return true if the thing they are looking
 * for was not found. The idea is to string a bunch
 * of these together as:
 *  if (not_a() || not_b() || not_c()) {
 *    report_err("Something went wrong");
 *  } else {
 *    process
 *  }
 * If the error occurs before the end of the input, then
 * an error message is reported via report_err(). In general
 * this should mean that the only error that would need to
 * be manually reported would be for syntax that is not
 * supported by one of the not_*() functions or a timeout.
 */
int not_found(unsigned char c);
int not_hex(unsigned short &hexval);
int not_int(int &val );
int not_str(const char *str, unsigned int len);
int not_str(const std::string &s);
int not_str(const char *str);
int not_float( float &val );

#endif