// Callsign-based display hints.  These values are intentionally kept out of
// CountryDat and LogBook so they cannot affect DXCC, B4, or AutoSeq matching.

#ifndef __CALLSIGNLOCATION_H
#define __CALLSIGNLOCATION_H

#include <QCoreApplication>
#include <QString>

class CallsignLocation
{
public:
  static inline QString tr(const char *sourceText, const char *disambiguation = Q_NULLPTR, int n = -1)
        { return QCoreApplication::translate("CallsignLocation", sourceText, disambiguation, n); }

  // Returns the Chinese amateur-call area inferred by TX-5DR's currently
  // installed rule set.  This is a callsign allocation hint, not a station's
  // reported grid or physical location.
  static QString chinaProvince(const QString& callsign, const QString& masterPrefix);
};

#endif
