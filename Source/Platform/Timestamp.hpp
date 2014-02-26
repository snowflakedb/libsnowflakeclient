/*
 * File:   Timestamp.hpp
 * Author: bdagevil
 *
 * Created on August 2, 2012, 9:04 AM
 */

#ifndef TIMESTAMP_HPP
#define	TIMESTAMP_HPP

#include <ctime>

#include "BasicTypes.hpp"
#include "pow10.hpp"
#include "DataTypeDefs.hpp"

namespace sf
{

/*
 * Class Timestamp
 */
class Timestamp final
{
public:
  /**
   * constant for seconds per day
   */
  static const sb4 SECONDS_PER_DAY;

  /**
   * constant for seconds per day
   */
  static const sb4 SECONDS_PER_HOUR;

  /**
   * Number of bits used to represent a timezone in LTY_TIMESTAMPTZ.
   * We use 14 bits, enough to represent 16384 values.
   * See https://snowflakecomputing.atlassian.net/wiki/display/EN/Timestamp+Data+Type
   */
  static const sb1 BITS_FOR_TIMEZONE = 14;

  /**
   * Mask used to extract the timezone from LTY_TIMESTAMPTZ
   */
  static const sb4 MASK_OF_TIMEZONE = (1 << BITS_FOR_TIMEZONE) - 1;

  /**
   * Maximum one-directional range of offset-based timezones (24 hours)
   */
  static const sb4 TIMEZONE_OFFSET_RANGE = 24 * 60;

  /**
   * Constructor
   */
  explicit Timestamp(LogicalType_t logicalType);

  inline ub1 getFractionalScale() const
  {
    return m_fractionalScale;
  }

  inline void setFractionalScale(ub1 fractionalScale)
  {
    this->m_fractionalScale = fractionalScale;
  }

  inline sb4 getFractionalSeconds() const
  {
    return m_fractionalSeconds;
  }

  inline void setFractionalSeconds(sb4 fractionalSeconds)
  {
    this->m_fractionalSeconds = fractionalSeconds;
  }

  inline sb8 getSecondsSinceEpoch() const
  {
    return m_secondsSinceEpoch;
  }

  inline void setSecondsSinceEpoch(sb8 secondsSinceEpoch)
  {
    this->m_secondsSinceEpoch = secondsSinceEpoch;
  }

  /**
   * Get the timezone offset of this timestamp, in seconds
   */
  inline sb4 getGMToffset() const
  {
    return m_GMToffset;
  }

  /**
   * Set the timezone offset of this timestamp, in seconds
   */
  inline void setGMToffset(sb4 GMToff)
  {
    this->m_GMToffset = GMToff;
  }

  /**
   * set the timestamp to default values which correspond to epoch time
   */
  inline void setDefault()
  {
    m_fractionalScale = 0;
    m_fractionalSeconds = 0;
    m_GMToffset = 0;
    m_secondsSinceEpoch = 0;
  }

  /**
   * Convert the fractional seconds to the scale
   *
   * @param scale
   *   target scale
   * @return
   *   fractional seconds in target scale
   */
  inline sb4 convertFractionalSeconds(sb1 scale) const
  {
    // same scale
    if (scale == m_fractionalScale)
    {
      return m_fractionalSeconds;
    }
    else if (scale > m_fractionalScale)
    {
      // requested scale more than existing scale
      return (m_fractionalSeconds *
          power10_ub4[scale - m_fractionalScale]);
    }
    else
    {
      // requested scale less than existing scale
      return (m_fractionalSeconds /
          power10_ub4[m_fractionalScale - scale]);
    }
  }

  /**
   * Convert the timestamp to number of fractional seconds since Epoch
   *
   * @param scale
   *   target scale
   * @return
   *   fractional seconds since Epoch
   */
  sb16 toFractionalSecondsSinceEpoch(sb1 scale) const;

  /**
   * Convert the fractional seconds since epoch to timestamp
   * @param fracSeconds
   *   number of fractional seconds
   * @param scale
   *   scale of fractional seconds
   */
  void fromFractionalSecondsSinceEpoch(sb16 fracSeconds, sb1 scale);

  /**
   * Convert the number of day since epoch to timestamp
   * @param daysSinceEpoch
   */
  inline void fromDaysSinceEpoch(sb4 daysSinceEpoch)
  {
    m_fractionalScale = 0;
    m_fractionalSeconds = 0;
    m_secondsSinceEpoch = static_cast<sb8>(daysSinceEpoch) * SECONDS_PER_DAY;
  }

#if defined(WIN32) || defined(_WIN64)
#else
  /**
   * Our version of mktime that uses timezone offset.
   * Glibc version of mktime uses time zone environment variable. It is not
   * thread safe and it requires setting TZ environment variable.
   */
  static inline sb8 convertTMToSecondsSinceEpoch(tm &tmV, bool adjustToUTC)
  {
    sb8 totalSecondsSinceEpoch;

    if (adjustToUTC)
    {
      sb8 gmtoff = tmV.tm_gmtoff;

      // convert the date time to seconds since epoch
      totalSecondsSinceEpoch = timegm(&tmV);

      // adjust with timezone offset
      totalSecondsSinceEpoch -= gmtoff;
    }
    else
    {
      totalSecondsSinceEpoch = timegm(&tmV);
    }

    return totalSecondsSinceEpoch;
  }
#endif
  /**
   * For a given logical type, defines if during parsing/conversion the time
   * should be converted into UTC.
   * For example, TIMESTAMP and TIMESTAMP_TZ are,
   * but DATE and TIMESTAMP_NTZ are not.
   */
  static bool needsUTCAdjustment(LogicalType_t type);

#if defined(WIN32) || defined(_WIN64)
#else
  /**
   * Converts TM to seconds, with UTC-local adjustment depending on ltype.
   */
  static inline sb8 convertTMToSecondsSinceEpoch(tm &tmV, LogicalType_t ltype)
  {
    return convertTMToSecondsSinceEpoch(tmV,
	                                needsUTCAdjustment(ltype));
  }
#endif
  /**
   * Converts a given timestamp into a TM representation.
   * Depending on the timestamp type, UTC or local time is used.
   */
  void convertToTM(struct tm *tmP)
  {
    switch (m_logicalType)
    {
      case LTY_TIMESTAMP_NTZ:
      case LTY_DATE:
		// Do localtime-oblivious conversion
#if defined(WIN32) || defined(_WIN64)
		gmtime_s(tmP, &m_secondsSinceEpoch);
#else
	    gmtime_r(&m_secondsSinceEpoch, tmP);
#endif
		break;
      default:
		// Do conversion to the localtime
#if defined(WIN32) || defined(_WIN64)
		localtime_s(tmP, &m_secondsSinceEpoch);
#else
	    localtime_r(&m_secondsSinceEpoch, tmP);
#endif
		break;
    }
  }

#if defined(WIN32) || defined(_WIN64)
#else
  /**
   * Our version of localtime that uses timezone offset.
   * Glibc version of localtime uses time zone environment variable. It is not
   * thread safe and it requires setting TZ environment variable.
   */
  static inline void convertSecondsSinceEpochToLocalTime(tm *tmP,
                                                  sb8 secondsSinceUTC,
                                                  sb4 GMToffset)
  {
    // add gmt offset to simulate the local time in UTC timezone
    secondsSinceUTC += GMToffset;

    // get the calendar time in UTC timezone
    gmtime_r(&secondsSinceUTC, tmP);

    // Change the timezone offset of the calendar time to the offset of the
    // local time zone
    tmP->tm_gmtoff = GMToffset;
  }
#endif
  /**
   * Set the time to epoch time
   * @param tmV
   */
  static void setTimeToEpoch(tm &tmV);

  /**
   * Convert the timestamp to the number of days since epoch
   * @return
   *   number of days since epoch
   */
  inline sb16 toDaysSinceEpoch() const
  {
    return m_secondsSinceEpoch / SECONDS_PER_DAY;
  }

private:

  /**
   * Seconds since Epoch
   */
  sb8 m_secondsSinceEpoch;

  /**
   * scale for the fractional seconds
   */
  ub1 m_fractionalScale;

  /**
   * Fractional seconds in scale
   */
  sb4 m_fractionalSeconds;

  /**
   * Time zone offset, in seconds
   */
  sb4 m_GMToffset;

  /**
   * What type this timestamp actually represents
   */
  LogicalType_t m_logicalType;
 };

} // namespace sf

#endif	/* TIMESTAMP_HPP */

