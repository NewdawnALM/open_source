#ifndef  TIMEVAL_H
#define  TIMEVAL_H

#include <sys/time.h>

#ifndef NULL
    #define NULL 0
#endif

class CTimeVal
{
public:
    CTimeVal() {  gettimeofday(&m_tv, NULL);  }

    CTimeVal(const timeval &_tv): m_tv(_tv) {}

    CTimeVal(const CTimeVal &rhs): m_tv(rhs.m_tv) {}

    CTimeVal& operator = (const timeval &_tv)
    {
        m_tv = _tv;
        return *this;
    }

    CTimeVal& operator = (const CTimeVal &rhs)
    {
        m_tv = rhs.m_tv;
        return *this;
    }

    bool operator < (const CTimeVal &rhs) const
    {
        return m_tv.tv_sec == rhs.m_tv.tv_sec ?
                m_tv.tv_usec < rhs.m_tv.tv_usec :
                m_tv.tv_sec < rhs.m_tv.tv_sec;
    }

    CTimeVal operator - (const CTimeVal &rhs) const
    {
        CTimeVal sub;
        sub.m_tv.tv_sec = m_tv.tv_sec - rhs.m_tv.tv_sec;
        sub.m_tv.tv_usec = m_tv.tv_usec - rhs.m_tv.tv_usec;

        if(sub.m_tv.tv_sec > 0 && sub.m_tv.tv_usec < 0)
        {
            --sub.m_tv.tv_sec;
            sub.m_tv.tv_usec += 1000000;
        }
        return sub;
    }

    CTimeVal AbsDiff(const CTimeVal &rhs) const
    {
        return *this < rhs ? rhs - *this : *this - rhs;
    }

    /**
     * [costTime description]
     * @return the cost time(millsecond in default).
     */
    long long CostTime(int type = MILLI) const
    {
        CTimeVal diff = CTimeVal().AbsDiff(*this);
        return type == SECOND ? diff.ToSeconds() :
                (type == MICRO ? diff.ToMicroSeconds() : diff.ToMilliSeconds());
    }

    /**
     * reset the time with this moment.
     */
    void Reset() {  gettimeofday(&m_tv, NULL);  }

    long long ToSeconds() const {  return m_tv.tv_sec;  }
    long long ToMilliSeconds() const {  return m_tv.tv_sec * 1000 + m_tv.tv_usec / 1000;  }
    long long ToMicroSeconds() const {  return m_tv.tv_sec * 1000000 + m_tv.tv_usec;  }

    enum TIMETYPE
    {
        SECOND, MILLI, MICRO
    };

private:
    timeval m_tv;
};

#endif  // TIMEVAL_H
