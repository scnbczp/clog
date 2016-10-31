
// CLog is a colorful console output and logrotate based logger for cpp.
// Author: scnbczp@163.com

#ifndef __CLOG_H__
#define __CLOG_H__

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <pthread.h>

// log priority
#define LOG_PRI_CRITICAL               10
#define LOG_PRI_WARN                   20
#define LOG_PRI_INFO                   30
#define LOG_PRI_DEBUG                  40

// log highlight option
#define LOG_HL_NONE                    0
#define LOG_HL_MARK                    1             // mark the log item with the same color background
#define LOG_HL_KEYT                    2             // mark the log item with red background and white text color

// log color option
#define LOG_CLR_NONE                   0             // use default log color
#define LOG_CLR_PID                    1             // use process-related log color
#define LOG_CLR_TID                    2             // use thread-related log color
#define LOG_CLR_GRAY                   -30
#define LOG_CLR_RED                    31
#define LOG_CLR_ORANGE                 -31
#define LOG_CLR_GREEN                  32
#define LOG_CLR_LGREEN                 -32
#define LOG_CLR_YELLOW                 -33
#define LOG_CLR_BLUE                   -34
#define LOG_CLR_PURPLE                 35
#define LOG_CLR_LPURPLE                -35
#define LOG_CLR_CYAN                   36
#define LOG_CLR_LCYAN                  -36

#define MAX_LOG_ITEM_LENGTH            1023
#define MAX_LOG_QUEUE_LENGTH           1024
#define MAX_LOG_QUEUE_TIME             60

#define LOGROTATE_CONFIG_PATH          "/etc/logrotate.d/"
#define LOGROTATE_ROTATE_SIZE          "50M"
#define LOGROTATE_ROTATE_NUM           4

class CLog
{
public:
	// Structure of log configuration context
	struct SLogContext
	{
		// The log level. Only those log items whose priority is less than
		// or equals to it will be printed to screen or written to the disk.
		int log_level = LOG_PRI_WARN;

		// The log color control code of current thread. If it's LOG_CLR_NONE, 
		// we will use default text color, and, if it's LOG_CLR_PID or LOG_CLR_TID,
		// we will use specific text color colordepending on the pid or tid.
		// Otherwise, the specified text color will be used.
		int log_color = LOG_CLR_PID;

		// Whether to print log items to screen or not. If true, the subsequent
		// log items will neither be pushed into the log queue nor written to the
		// log files, until this is set to false.
		bool b_log_screen = true;

		// Whether to configure and use logrotate
		bool b_log_rotate = false;

		// The full path of the log file.
		char log_file_name[PATH_MAX + 1];
	};

	// ------------------------------------------------
	// Get the singleton instance of CLog
	// ------------------------------------------------
	static CLog& getInstance()
	{
		// NOTE: Only since C++11 standard is the thread safe of static local variable ensured.
		static CLog instance;
		return instance;
	}

	// ------------------------------------------------
	// Configure the log context
	// ------------------------------------------------
	void setContext(const SLogContext* ctx);

	// ------------------------------------------------
	// Change the log level option
	// ------------------------------------------------
	void setLogLevel(const int log_level);

	// ------------------------------------------------
	// Log a new item
	// ------------------------------------------------
	void LOG(const int log_priority, const int log_highlight, const char* log_format, ...)
	{
		va_list vargs;
		va_start(vargs, log_format);
		_log(log_priority, log_highlight, log_format, &vargs);
		va_end(vargs);
	}

	// ------------------------------------------------
	// Log a new CRITICAL item
	// ------------------------------------------------
	void CRITICAL(const char* log_format, ...)
	{
		va_list vargs;
		va_start(vargs, log_format);
		_log(LOG_PRI_CRITICAL, LOG_HL_NONE, log_format, &vargs);
		va_end(vargs);
	}

	// ------------------------------------------------
	// Log a new WARN item
	// ------------------------------------------------
	void WARN(const char* log_format, ...)
	{
		va_list vargs;
		va_start(vargs, log_format);
		_log(LOG_PRI_WARN, LOG_HL_NONE, log_format, &vargs);
		va_end(vargs);
	}

	// ------------------------------------------------
	// Log a new INFO item
	// ------------------------------------------------
	void INFO(const char* log_format, ...)
	{
		va_list vargs;
		va_start(vargs, log_format);
		_log(LOG_PRI_INFO, LOG_HL_NONE, log_format, &vargs);
		va_end(vargs);
	}

	// ------------------------------------------------
	// Log a new DEBUG item
	// ------------------------------------------------
	void DEBUG(const char* log_format, ...)
	{
		va_list vargs;
		va_start(vargs, log_format);
		_log(LOG_PRI_DEBUG, LOG_HL_NONE, log_format, &vargs);
		va_end(vargs);
	}

private:
	// Log queue
	struct SLogQueue
	{
		char items[MAX_LOG_QUEUE_LENGTH][MAX_LOG_ITEM_LENGTH + 1];
		iovec iovs[MAX_LOG_QUEUE_LENGTH];
		int length = 0;
		int flush_sec = 0;
		int log_fd = -1;
	} m_log_queue;

	// Thread mutex lock for m_log_queue
	pthread_mutex_t m_mutex;

	// Current log configuration context
	SLogContext m_ctx;

	// ------------------------------------------------
	// Constructor
	// ------------------------------------------------
	CLog();

	// ------------------------------------------------
	// Copy Constructor
	// NOTE: DON'T implement it.
	// ------------------------------------------------
	CLog(const CLog&);

	// ------------------------------------------------
	// Operator Overloading
	// NOTE: DON'T implement it.
	// ------------------------------------------------
	CLog& operator=(const CLog&);

	// ------------------------------------------------
	// Destructor
	// ------------------------------------------------
	~CLog();

	// ------------------------------------------------
	// Get thread ID
	// ------------------------------------------------
	static pid_t _gettid()
	{
		return syscall(SYS_gettid);
	}

	// ------------------------------------------------
	// Get current timestamp in sec or usec
	// ------------------------------------------------
	static long long _getTimestamp(bool b_usec = false)
	{
		timeval tv;
		gettimeofday(&tv, nullptr);
		if(b_usec)
			return tv.tv_sec * 1000000 + tv.tv_usec;
		else
			return tv.tv_sec;
	}

	// ------------------------------------------------
	// Get parsed log color option
	// ------------------------------------------------
	int _getLogColor()
	{
		static const int log_colors[10] = {
			LOG_CLR_RED,     LOG_CLR_BLUE,
			LOG_CLR_GREEN,   LOG_CLR_LGREEN,
			LOG_CLR_LPURPLE, LOG_CLR_ORANGE,
			LOG_CLR_YELLOW,  LOG_CLR_CYAN,
			LOG_CLR_PURPLE,  LOG_CLR_LCYAN
		};
		if(m_ctx.log_color != LOG_CLR_PID && m_ctx.log_color != LOG_CLR_TID)
			return m_ctx.log_color;
		else
		{
			pid_t pid = m_ctx.log_color == LOG_CLR_TID ? _gettid() : getpid();
			return log_colors[pid % 10];
		}
	}

	// ------------------------------------------------
	// Configure logrotate
	// NOTE: Call it ONLY inside an external mutex lock
	// ------------------------------------------------
	void _configLogrotate();

	// ------------------------------------------------
	// Format a log item
	// ------------------------------------------------
	void _formatLogItem(char* buf, const int log_priority, const char* log_format, va_list* vargs);

	// ------------------------------------------------
	// Print a log item to screen
	// ------------------------------------------------
	void _printLogItem(const char* buf, const int log_highlight);

	// ------------------------------------------------
	// Flush log queue to log files
	// NOTE: Call it ONLY inside an external mutex lock
	// ------------------------------------------------
	void _flushLogQueue(bool force = false);

	// ----------------------------------------------------------------
	// Log a new item internally
	// ----------------------------------------------------------------
	void _log(const int log_priority, const int log_highlight, const char* log_format, va_list* vargs);
};

#endif