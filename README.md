# CLog
CLog is a common logger for cpp programs, which is firstly used in our distributed file system BoSS. It's pretty simple to use, but its main features gave us much convenience during the development of our multi-process/thread programs.

## Features
* Colorful console output based on process/thread ids, or just a particular pre-defined text color
* Highlight on particular log lines in console output
* Efficient log queue in memory, which could be written to log file in local order periodically
* Multi-process/thread supported and thread safe
* Optional and automatic configuration of logrotate

## Usage
### For console output
    #include "clog.h"
    
    // Config process related log context
    CLog::SLogContext ctx;
    ctx.log_level = LOG_PRI_WARN;
    ctx.log_color = LOG_CLR_PID;
    ctx.b_log_screen = true;
    ctx.b_log_rotate = false;
    strcpy(ctx.log_file_name, "");
    
    // Get instance and output log lines
    CLog::getInstance().setContext(&ctx);
    CLog::getInstance().WARN("Some warning information here.");
    CLog::getInstance().LOG(LOG_PRI_WARN, LOG_HL_MARK, "Highlight information here!");
### For log file output
    // Just config log context like this
    CLog::SLogContext ctx;
    ctx.log_level = LOG_PRI_WARN;
    ctx.b_log_screen = false;
    ctx.b_log_rotate = true;
    strcpy(ctx.log_file_name, "/tmp/foobar.log");

## Attention
CLog supports multi-process programs. However, you may need to firstly call CLog::flushLogs() explicitly to flush log items in memory back to the log file before fork().