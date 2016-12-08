
// CLog is a colorful console output and logrotate based logger for cpp.
// Author: scnbczp@163.com

#include "clog.h"

CLog::CLog()
{
	int ret = pthread_mutex_init(&m_mutex, nullptr);
	if(ret != 0)
		printf("ERROR: CLog module failed to be initialized.\n");

	ret = pthread_mutex_lock(&m_mutex);
	if(ret == 0)
	{
		memset(&m_log_queue, 0, sizeof(m_log_queue));
		for(int i = 0; i < MAX_LOG_QUEUE_LENGTH; ++i)
		{
			m_log_queue.iovs[i].iov_base = m_log_queue.items[i];
			m_log_queue.iovs[i].iov_len = 0;
		}
		m_log_queue.length = 0;
		m_log_queue.flush_sec = _getTimestamp();
		m_log_queue.log_fd = -1;
		pthread_mutex_unlock(&m_mutex);
	}

	SLogContext ctx_default;
	ctx_default.log_level = LOG_PRI_WARN;
	ctx_default.log_color = LOG_CLR_PID;
	ctx_default.b_log_screen = true;
	ctx_default.b_log_rotate = false;
	strcpy(ctx_default.log_file_name, "");
	setContext(&ctx_default);
}

CLog::~CLog()
{
	int ret = pthread_mutex_lock(&m_mutex);
	if(ret == 0)
	{
		_flushLogQueue(true);
		if(m_log_queue.log_fd != -1)
		{
			close(m_log_queue.log_fd);
			m_log_queue.log_fd = -1;
		}
		pthread_mutex_unlock(&m_mutex);
	}
	pthread_mutex_destroy(&m_mutex);
}

void CLog::setContext(const SLogContext* ctx)
{
	if((!m_ctx.b_log_screen && ctx->b_log_screen) || (strcmp(ctx->log_file_name, m_ctx.log_file_name) != 0))
	{
		int ret = pthread_mutex_lock(&m_mutex);
		if(ret == 0)
		{
			_flushLogQueue(true);
			if(m_log_queue.log_fd != -1)
			{
				close(m_log_queue.log_fd);
				m_log_queue.log_fd = -1;
			}
			pthread_mutex_unlock(&m_mutex);
		}
	}
	memcpy(&m_ctx, ctx, sizeof(m_ctx));

	int ret = pthread_mutex_lock(&m_mutex);
	if(ret == 0)
	{
		_configLogrotate();
		pthread_mutex_unlock(&m_mutex);
	}
}

void CLog::setLogLevel(const int log_level)
{
	m_ctx.log_level = log_level;
}

void CLog::flushLogs()
{
	int ret = pthread_mutex_lock(&m_mutex);
	if(ret == 0)
	{
		_flushLogQueue(true);
		pthread_mutex_unlock(&m_mutex);
	}
}

void CLog::_configLogrotate()
{
	// NOTE: Call it ONLY inside an external mutex lock

	int flen = strlen(m_ctx.log_file_name);
	if(flen == 0 || m_ctx.log_file_name[0] != '/')
		return;
	const char *p_slash = strrchr(m_ctx.log_file_name, '/');
	if(p_slash == m_ctx.log_file_name + flen - 1)
		return;

	char config_file[PATH_MAX + 1];
	sprintf(config_file, "%s/%s.conf", LOGROTATE_CONFIG_PATH, p_slash + 1);

	if(!m_ctx.b_log_rotate)
		unlink(config_file);
	else
	{
		int cfd = open(config_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
		if(cfd != -1)
		{
			char buf[1024];
			sprintf(buf, "%s\n{\nmissingok\nnotifempty\nnocompress\ncopytruncate\nnodateext\nstart 1\n"
				"rotate %d\nsize %s\n}", m_ctx.log_file_name, LOGROTATE_ROTATE_NUM, LOGROTATE_ROTATE_SIZE);
			write(cfd, buf, strlen(buf));
			close(cfd);
		}
	}
}

void CLog::_formatLogItem(char* buf, const int log_priority, const char* log_format, va_list* vargs)
{
	pid_t pid = m_ctx.log_color == LOG_CLR_TID ? _gettid() : getpid();

	long long ts = _getTimestamp(true);
	long long tsec = ts / 1000000;
	long long tusec = ts - tsec * 1000000;

	char log_type = 'W';
	switch(log_priority)
	{
	case LOG_PRI_WARN: log_type = 'W'; break;
	case LOG_PRI_INFO: log_type = 'I'; break;
	case LOG_PRI_DEBUG: log_type = 'D'; break;
	case LOG_PRI_CRITICAL: log_type = 'C'; break;
	}

	if(m_ctx.b_log_screen)
	{
		snprintf(buf, 23, "[%-6d %03lld.%06lld %c] ", pid, tsec & 0xFF, tusec, log_type);
		vsnprintf(buf + 22, MAX_LOG_ITEM_LENGTH - 22, log_format, *vargs);
	}
	else
	{
		char buf_dtt[16];
		time_t tt = (time_t)tsec;
		tm *tmt = localtime(&tt);
		strftime(buf_dtt, sizeof(buf_dtt), "%y%m%d:%H%M%S", tmt);

		memset(buf, 0, MAX_LOG_ITEM_LENGTH + 1);
		snprintf(buf, 33, "[%-6d %s.%06lld %c] ", pid, buf_dtt, tusec, log_type);
		vsnprintf(buf + 32, MAX_LOG_ITEM_LENGTH - 32 - 1, log_format, *vargs);
		int slen = strlen(buf);
		buf[slen] = '\n';
		buf[slen + 1] = 0;
	}
}

void CLog::_printLogItem(const char* buf, const int log_highlight)
{
	int log_color = _getLogColor();

	if(log_highlight == LOG_HL_KEYT)
		printf("\033[1;37;41m%s\033[0m\n", buf);
	else if(log_highlight == LOG_HL_MARK)
	{
		switch(log_color)
		{
		case LOG_CLR_GRAY:
			printf("\033[30;47m%s\033[0m\n", buf); break;
		case LOG_CLR_RED:
		case LOG_CLR_ORANGE:
		case LOG_CLR_PURPLE:
		case LOG_CLR_LPURPLE:
			printf("\033[30;45m%s\033[0m\n", buf); break;
		case LOG_CLR_YELLOW:
			printf("\033[30;43m%s\033[0m\n", buf); break;
		case LOG_CLR_BLUE:
			printf("\033[30;44m%s\033[0m\n", buf); break;
		case LOG_CLR_CYAN:
		case LOG_CLR_LCYAN:
			printf("\033[30;46m%s\033[0m\n", buf); break;
		case LOG_CLR_GREEN:
		case LOG_CLR_LGREEN:
		default:
			printf("\033[30;42m%s\033[0m\n", buf); break;
		}
	}
	else
	{
		if(log_color < 0)
			printf("\033[1;%dm%s\033[0m\n", -1 * log_color, buf);
		else
			printf("\033[%dm%s\033[0m\n", log_color, buf);
	}
}

void CLog::_flushLogQueue(bool force)
{
	// NOTE: Call it ONLY inside an external mutex lock

	if(m_log_queue.length == 0)
		return;
	long long tsec = _getTimestamp();
	if(!force && m_log_queue.length < MAX_LOG_QUEUE_LENGTH && tsec - m_log_queue.flush_sec < MAX_LOG_QUEUE_TIME)
		return;

	if(m_log_queue.log_fd == -1)
		m_log_queue.log_fd = open(m_ctx.log_file_name, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if(m_log_queue.log_fd != -1)
	{
		for(int i = 0; i < m_log_queue.length; ++i)
			m_log_queue.iovs[i].iov_len = strlen(m_log_queue.items[i]);

		writev(m_log_queue.log_fd, m_log_queue.iovs, m_log_queue.length);
		_configLogrotate();
	}

	m_log_queue.length = 0;
	m_log_queue.flush_sec = tsec;
}

void CLog::_log(const int log_priority, const int log_highlight, const char* log_format, va_list* vargs)
{
	if(log_priority > m_ctx.log_level)
		return;

	if(m_ctx.b_log_screen)
	{
		char buf[MAX_LOG_ITEM_LENGTH + 1];
		_formatLogItem(buf, log_priority, log_format, vargs);
		_printLogItem(buf, log_highlight);
	}
	else
	{
		int ret = pthread_mutex_lock(&m_mutex);
		if(ret == 0)
		{
			_flushLogQueue();
			_formatLogItem(m_log_queue.items[m_log_queue.length++], log_priority, log_format, vargs);
			pthread_mutex_unlock(&m_mutex);
		}
	}
}