/*
 * Copyright (C) 2009-2022 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               Terminal class.
 */

#include "terminal.h"

#include <core/log.h>
#include <core/utility.h>

#include <kernel/status.h>
#include <kernel/thread.h>
#include <kernel/user_file.h>

#include <services/terminal_service.h>

#include <assert.h>

#include <array>

static constexpr uint64_t kSupportedUserFileOps =
    USER_FILE_SUPPORTED_OP_READ |
    USER_FILE_SUPPORTED_OP_WRITE |
    USER_FILE_SUPPORTED_OP_INFO |
    USER_FILE_SUPPORTED_OP_REQUEST |
    USER_FILE_SUPPORTED_OP_WAIT |
    USER_FILE_SUPPORTED_OP_UNWAIT;

Terminal::Terminal(Kiwi::Core::Connection connection) :
    m_connection         (std::move(connection)),
    m_escaped            (false),
    m_inhibited          (false),
    m_inputBufferStart   (0),
    m_inputBufferSize    (0),
    m_inputBufferLines   (0)
{
    auto toControl = [] (unsigned char ch) -> cc_t {
        return ch & 0x1f;
    };

    /* Initialise terminal settings to default. */
    m_termios.c_iflag      = ICRNL;
    m_termios.c_oflag      = OPOST | ONLCR;
    m_termios.c_cflag      = CREAD | CS8 | HUPCL | CLOCAL;
    m_termios.c_lflag      = ICANON | IEXTEN | ISIG | ECHO | ECHOE | ECHONL;
    m_termios.c_cc[VEOF]   = toControl('D');
    m_termios.c_cc[VEOL]   = _POSIX_VDISABLE;
    m_termios.c_cc[VERASE] = toControl('H');
    m_termios.c_cc[VINTR]  = toControl('C');
    m_termios.c_cc[VKILL]  = toControl('U');
    m_termios.c_cc[VMIN]   = _POSIX_VDISABLE;
    m_termios.c_cc[VQUIT]  = toControl('\\');
    m_termios.c_cc[VSTART] = toControl('Q');
    m_termios.c_cc[VSTOP]  = toControl('S');
    m_termios.c_cc[VSUSP]  = toControl('Z');
    m_termios.c_cc[VTIME]  = _POSIX_VDISABLE;
    m_termios.c_cc[VLNEXT] = toControl('V');
    m_termios.c_ispeed     = B38400;
    m_termios.c_ospeed     = B38400;
    m_winsize.ws_col       = 80;
    m_winsize.ws_row       = 25;
}

Terminal::~Terminal() {}

void Terminal::run() {
    status_t ret;

    ret = kern_user_file_create(
        FILE_TYPE_CHAR, FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0,
        kSupportedUserFileOps,
        m_userFileConnection.attach(), m_userFile.attach());
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create user file: %" PRId32, ret);
        return;
    }

    m_thread = std::thread([this] () { thread(); });
}

void Terminal::thread() {
    status_t ret;

    core_log(CORE_LOG_DEBUG, "terminal started");

    std::array<object_event_t, 4> events;
    events[0].handle = m_connection.handle();
    events[0].event  = CONNECTION_EVENT_HANGUP;
    events[1].handle = events[0].handle;
    events[1].event  = CONNECTION_EVENT_MESSAGE;
    events[2].handle = m_userFileConnection;
    events[2].event  = CONNECTION_EVENT_HANGUP;
    events[3].handle = m_userFileConnection;
    events[3].event  = CONNECTION_EVENT_MESSAGE;

    bool exit = false;

    while (!exit) {
        ret = kern_object_wait(events.data(), events.size(), 0, -1);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to wait for events: %" PRId32, ret);
            continue;
        }

        for (object_event_t &event : events) {
            if (event.flags & OBJECT_EVENT_SIGNALLED) {
                exit = handleEvent(event);
            } else if (event.flags & OBJECT_EVENT_ERROR) {
                core_log(CORE_LOG_WARN, "error signalled on event %" PRId32 "/%" PRId32, event.handle, event.event);
            }

            event.flags &= ~(OBJECT_EVENT_SIGNALLED | OBJECT_EVENT_ERROR);
        }
    }

    core_log(CORE_LOG_DEBUG, "thread exiting");
    m_thread.detach();
    delete this;
}

bool Terminal::handleEvent(object_event_t &event) {
    if (event.handle == m_connection.handle()) {
        switch (event.event) {
            case CONNECTION_EVENT_HANGUP:
                core_log(CORE_LOG_DEBUG, "client hung up, closing terminal");
                return true;

            case CONNECTION_EVENT_MESSAGE:
                return handleClientMessages();

            default:
                core_unreachable();

        }
    } else if (event.handle == m_userFileConnection) {
        switch (event.event) {
            case CONNECTION_EVENT_HANGUP:
                /* This shouldn't happen since we have the file open ourself. */
                core_log(CORE_LOG_ERROR, "user file connection hung up unexpectedly");
                return true;

            case CONNECTION_EVENT_MESSAGE:
                return handleFileMessages();

            default:
                core_unreachable();

        }
    } else {
        core_unreachable();
    }

    return false;
}

bool Terminal::handleClientMessages() {
    while (true) {
        status_t ret;

        Kiwi::Core::Message message;
        ret = m_connection.receive(0, message);
        if (ret == STATUS_WOULD_BLOCK) {
            return false;
        } else if (ret == STATUS_CONN_HUNGUP) {
            return true;
        } else if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to receive client message: %" PRId32, ret);
            return false;
        }

        assert(message.type() == Kiwi::Core::Message::kRequest);

        Kiwi::Core::Message reply;

        uint32_t id = message.id();
        switch (id) {
            case TERMINAL_REQUEST_OPEN_HANDLE:
                reply = handleClientOpenHandle(message);
                break;
            case TERMINAL_REQUEST_INPUT:
                reply = handleClientInput(message);
                break;
            default:
                core_log(CORE_LOG_WARN, "unhandled request %" PRIu32, id);
                break;
        }

        if (reply.isValid()) {
            ret = m_connection.reply(reply);

            if (ret != STATUS_SUCCESS)
                core_log(CORE_LOG_WARN, "failed to send reply: %" PRId32, ret);
        }
    }
}

Kiwi::Core::Message Terminal::handleClientOpenHandle(Kiwi::Core::Message &request) {
    auto requestData = request.data<terminal_request_open_handle_t>();

    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(terminal_reply_open_handle_t))) {
        core_log(CORE_LOG_ERROR, "failed to create message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<terminal_reply_open_handle_t>();
    replyData->result = STATUS_SUCCESS;

    handle_t handle;
    status_t ret = kern_file_reopen(m_userFile, requestData->access, 0, &handle);
    if (ret != STATUS_SUCCESS) {
        replyData->result = STATUS_TRY_AGAIN;
    } else {
        reply.attachHandle(handle, true);
    }

    return reply;
}

Kiwi::Core::Message Terminal::handleClientInput(Kiwi::Core::Message &request) {
    auto requestData   = request.data<unsigned char>();
    size_t requestSize = request.size();

    for (size_t i = 0; i < requestSize; i++)
        addInput(requestData[i]);

    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(terminal_reply_input_t))) {
        core_log(CORE_LOG_ERROR, "failed to create message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<terminal_reply_input_t>();
    replyData->result = STATUS_SUCCESS;

    return reply;
}

bool Terminal::handleFileMessages() {
    status_t ret;

    while (true) {
        ipc_message_t message;
        ret = kern_connection_receive(m_userFileConnection, &message, nullptr, 0);
        if (ret == STATUS_WOULD_BLOCK) {
            return false;
        } else if (ret == STATUS_CONN_HUNGUP) {
            core_log(CORE_LOG_ERROR, "user file connection hung up unexpectedly");
            return true;
        } else if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to receive file message: %" PRId32, ret);
            return false;
        }

        std::unique_ptr<uint8_t[]> data;
        if (message.size > 0) {
            data.reset(new uint8_t[message.size]);

            ret = kern_connection_receive_data(m_userFileConnection, data.get());
            if (ret != STATUS_SUCCESS) {
                core_log(CORE_LOG_WARN, "failed to receive file message data: %" PRId32, ret);
                return false;
            }
        }

        switch (message.id) {
            case USER_FILE_OP_READ:
                ret = handleFileRead(message);
                break;
            case USER_FILE_OP_WRITE:
                ret = handleFileWrite(message, data.get());
                break;
            case USER_FILE_OP_INFO:
                ret = handleFileInfo(message);
                break;
            case USER_FILE_OP_REQUEST:
                ret = handleFileRequest(message, data.get());
                break;
            case USER_FILE_OP_WAIT:
                ret = handleFileWait(message);
                break;
            case USER_FILE_OP_UNWAIT:
                ret = handleFileUnwait(message);
                break;
            default:
                core_unreachable();
        }

        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to send file message %" PRIu32 ": %" PRId32, message.id, ret);
            return false;
        }
    }
}

static ipc_message_t initializeFileReply(uint32_t id, uint64_t serial) {
    ipc_message_t reply = {};
    reply.id = id;
    reply.args[USER_FILE_MESSAGE_ARG_SERIAL] = serial;
    return reply;
}

static ipc_message_t initializeFileReply(const ipc_message_t &message) {
    return initializeFileReply(message.id, message.args[USER_FILE_MESSAGE_ARG_SERIAL]);
}

status_t Terminal::handleFileRead(const ipc_message_t &message) {
    ReadOperation op;
    op.serial   = message.args[USER_FILE_MESSAGE_ARG_SERIAL];
    op.size     = message.args[USER_FILE_MESSAGE_ARG_READ_SIZE];
    op.canon    = m_termios.c_lflag & ICANON;
    op.nonblock = message.args[USER_FILE_MESSAGE_ARG_FLAGS] & FILE_NONBLOCK;

    if (!readBuffer(op)) {
        /* Cannot be completed yet, queue it. */
        m_pendingReads.emplace_back(op);
    }

    return STATUS_SUCCESS;
}

status_t Terminal::handleFileWrite(const ipc_message_t &message, const void *data) {
    /* Pass this on to the client. */
    status_t ret = sendOutput(data, message.size);

    ipc_message_t reply = initializeFileReply(message);
    reply.args[USER_FILE_MESSAGE_ARG_WRITE_STATUS] = ret;
    reply.args[USER_FILE_MESSAGE_ARG_WRITE_SIZE]   = (ret == STATUS_SUCCESS) ? message.size : 0;

    return kern_connection_send(m_userFileConnection, &reply, nullptr, INVALID_HANDLE, -1);
}

status_t Terminal::handleFileInfo(const ipc_message_t &message) {
    file_info_t info;
    memset(&info, 0, sizeof(info));
    info.block_size = 4096;
    info.links      = 1;

    ipc_message_t reply = initializeFileReply(message);
    reply.size = sizeof(file_info_t);

    return kern_connection_send(m_userFileConnection, &reply, &info, INVALID_HANDLE, -1);
}

status_t Terminal::handleFileRequest(const ipc_message_t &message, const void *data) {
    status_t ret;

    std::unique_ptr<uint8_t[]> outData;
    size_t outDataSize = 0;

    unsigned request = message.args[USER_FILE_MESSAGE_ARG_REQUEST_NUM];
    switch (request) {
        case TIOCDRAIN: {
            /* tcdrain(int fd) - nothing to do, we don't buffer any output. */
            ret = STATUS_SUCCESS;
            break;
        }
        case TCXONC: {
            /* tcflow(int fd, int action). */
            if (message.size != sizeof(int)) {
                ret = STATUS_INVALID_ARG;
                break;
            }

            int action = *reinterpret_cast<const int *>(data);

            switch (action) {
                case TCIOFF:
                    addInput(m_termios.c_cc[VSTOP]);
                    ret = STATUS_SUCCESS;
                    break;
                case TCION:
                    addInput(m_termios.c_cc[VSTART]);
                    ret = STATUS_SUCCESS;
                    break;
                case TCOOFF:
                case TCOON:
                    ret = STATUS_NOT_IMPLEMENTED;
                    break;
                default:
                    ret = STATUS_INVALID_ARG;
                    break;
            }

            break;
        }
        case TCFLSH: {
            /* tcflush(int fd, int action) - TODO. */
            if (message.size != sizeof(int)) {
                ret = STATUS_INVALID_ARG;
                break;
            }

            int action = *reinterpret_cast<const int *>(data);

            /* No output buffering so just need to deal with input. */
            switch (action) {
                case TCIFLUSH:
                case TCIOFLUSH:
                    clearBuffer();
                    [[fallthrough]];
                case TCOFLUSH:
                    ret = STATUS_SUCCESS;
                    break;
                default:
                    ret = STATUS_INVALID_ARG;
                    break;
            }

            break;
        }
        case TCGETA: {
            /* tcgetattr(int fd, struct termios *tiop). */
            outDataSize = sizeof(struct termios);
            outData.reset(new uint8_t[outDataSize]);

            memcpy(outData.get(), &m_termios, sizeof(m_termios));

            ret = STATUS_SUCCESS;
            break;
        }
        case TCSETA:
        case TCSETAW:
        case TCSETAF: {
            /* tcsetattr(int fd, TCSANOW / TCSADRAIN / TCSAFLUSH). */
            if (message.size != sizeof(struct termios)) {
                ret = STATUS_INVALID_ARG;
                break;
            }

            /* No output buffering to flush, just input. */
            if (request == TCSETAF)
                clearBuffer();

            memcpy(&m_termios, data, sizeof(m_termios));

            ret = STATUS_SUCCESS;
            break;
        }
        case TIOCGPGRP: {
            /* tcgetpgrp(int fd) - TODO. */
            ret = STATUS_NOT_IMPLEMENTED;
            break;
        }
        case TIOCSPGRP: {
            /* tcsetpgrp(int fd, pid_t pgid) - TODO. */
            ret = STATUS_NOT_IMPLEMENTED;
            break;
        }
        case TIOCGWINSZ: {
            outDataSize = sizeof(struct winsize);
            outData.reset(new uint8_t[outDataSize]);

            memcpy(outData.get(), &m_winsize, sizeof(m_winsize));

            ret = STATUS_SUCCESS;
            break;
        }
        case TIOCSWINSZ: {
            if (message.size != sizeof(struct winsize)) {
                ret = STATUS_INVALID_ARG;
                break;
            }

            memcpy(&m_winsize, data, sizeof(m_winsize));

            ret = STATUS_SUCCESS;
            break;
        }
        default: {
            ret = STATUS_INVALID_REQUEST;
            break;
        }
    }

    ipc_message_t reply = initializeFileReply(message);
    reply.size = outDataSize;
    reply.args[USER_FILE_MESSAGE_ARG_REQUEST_STATUS] = ret;

    return kern_connection_send(m_userFileConnection, &reply, outData.get(), INVALID_HANDLE, -1);
}

status_t Terminal::handleFileWait(const ipc_message_t &message) {
    ipc_message_t reply = initializeFileReply(message);
    reply.args[USER_FILE_MESSAGE_ARG_EVENT_NUM]    = message.args[USER_FILE_MESSAGE_ARG_EVENT_NUM];
    reply.args[USER_FILE_MESSAGE_ARG_EVENT_STATUS] = STATUS_SUCCESS;

    bool sendReply = false;

    switch (message.args[USER_FILE_MESSAGE_ARG_EVENT_NUM]) {
        case FILE_EVENT_READABLE:
            sendReply = isReadable();
            if (!sendReply) {
                m_readEvents.emplace_back(message.args[USER_FILE_MESSAGE_ARG_SERIAL]);
                break;
            }

            break;
        case FILE_EVENT_WRITABLE:
            /* Always writable. */
            sendReply = true;
            break;
        default:
            reply.args[USER_FILE_MESSAGE_ARG_EVENT_STATUS] = STATUS_INVALID_EVENT;
            sendReply = true;
            break;
    }

    return (sendReply)
        ? kern_connection_send(m_userFileConnection, &reply, nullptr, INVALID_HANDLE, -1)
        : STATUS_SUCCESS;
}

status_t Terminal::handleFileUnwait(const ipc_message_t &message) {
    if (message.args[USER_FILE_MESSAGE_ARG_EVENT_NUM] == FILE_EVENT_READABLE) {
        for (auto it = m_readEvents.begin(); it != m_readEvents.end(); ++it) {
            if (*it == message.args[USER_FILE_MESSAGE_ARG_EVENT_SERIAL]) {
                m_readEvents.erase(it);
                break;
            }
        }
    }

    return STATUS_SUCCESS;
}

void Terminal::signalReadEvents() {
    if (isReadable()) {
        while (!m_readEvents.empty()) {
            uint64_t serial = m_readEvents.back();
            m_readEvents.pop_back();

            ipc_message_t reply = initializeFileReply(USER_FILE_OP_WAIT, serial);
            reply.args[USER_FILE_MESSAGE_ARG_EVENT_NUM]    = FILE_EVENT_READABLE;
            reply.args[USER_FILE_MESSAGE_ARG_EVENT_STATUS] = STATUS_SUCCESS;

            status_t ret = kern_connection_send(m_userFileConnection, &reply, nullptr, INVALID_HANDLE, -1);
            if (ret != STATUS_SUCCESS && ret != STATUS_CANCELLED)
                core_log(CORE_LOG_WARN, "failed to send file message %" PRIu32 ": %" PRId32, reply.id, ret);
        }
    }
}

status_t Terminal::sendOutput(const void *data, size_t size) {
    status_t ret;

    Kiwi::Core::Message signal;
    if (signal.createSignal(TERMINAL_SIGNAL_OUTPUT, size)) {
        memcpy(signal.data<void>(), data, size);

        ret = m_connection.signal(signal);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to send signal: %" PRId32, ret);
            ret = STATUS_DEVICE_ERROR;
        }
    } else {
        core_log(CORE_LOG_ERROR, "failed to create message");
        ret = STATUS_NO_MEMORY;
    }

    return ret;
}

void Terminal::addInput(unsigned char value) {
    uint16_t ch = value;

    /* Strip character to 7-bits if required. */
    if (m_termios.c_iflag & ISTRIP)
        ch &= 0x7f;

    /* Perform extended processing if required. For now we only support escaping
     * the next character (VLNEXT). */
    if (m_termios.c_lflag & IEXTEN) {
        if (m_escaped) {
            /* Escape the current character. */
            ch |= kChar_Escaped;
            m_escaped = false;
        } else if (isControlChar(ch, VLNEXT)) {
            m_escaped = true;
            return;
        }
    }

    /* Handle CR/NL characters. */
    switch (ch) {
        case '\r':
            if (m_termios.c_iflag & IGNCR) {
                /* Ignore it. */
                return;
            } else if (m_termios.c_iflag & ICRNL) {
                /* Convert it to a newline. */
                ch = '\n';
            }

            break;
        case '\n':
            if (m_termios.c_iflag & INLCR) {
                /* Convert it to a carriage return. */
                ch = '\r';
            }

            break;
    }

    /* Check for output control characters. */
    if (m_termios.c_iflag & IXON) {
        if (isControlChar(ch, VSTOP)) {
            m_inhibited = true;
            return;
        } else if (m_inhibited) {
            /* Restart on any character if IXANY is set, but don't ignore it. */
            if (m_termios.c_iflag & IXANY) {
                m_inhibited = false;
            } else if (isControlChar(ch, VSTART)) {
                m_inhibited = false;
                return;
            }
        }
    }

    if (m_inhibited)
        return;

    /* Perform canonical-mode processing. */
    if (m_termios.c_lflag & ICANON) {
        if (isControlChar(ch, VERASE)) {
            /* Erase one character. */
            if (eraseChar()) {
                /* ECHOE means print an erasing backspace. */
                if (m_termios.c_lflag & ECHOE) {
                    echoInput('\b', true);
                    echoInput(' ', true);
                    echoInput('\b', true);
                } else {
                    echoInput(ch, false);
                }
            }

            return;
        } else if (isControlChar(ch, VKILL)) {
            size_t erased = eraseLine();
            if (erased > 0) {
                if (m_termios.c_lflag & ECHOE) {
                    while (erased--) {
                        echoInput('\b', true);
                        echoInput(' ', true);
                        echoInput('\b', true);
                    }
                }

                if (m_termios.c_lflag & ECHOK)
                    echoInput('\n', true);
            }

            return;
        }
    }

    /* Generate signals on INTR and QUIT if ISIG is set. */
    if (m_termios.c_lflag & ISIG) {
        if (isControlChar(ch, VINTR)) {
            /* TODO: Send signal. */
            return;
        } else if (isControlChar(ch, VQUIT)) {
            /* TODO: Send signal. */
            return;
        }
    }

    /* Check for newline/EOF. */
    if (ch == '\n' || isControlChar(ch, VEOF) || isControlChar(ch, VEOL)) {
        if (isControlChar(ch, VEOF))
            ch |= kChar_Eof;

        ch |= kChar_NewLine;
    }

    if (m_inputBufferSize == kInputBufferMax) {
        // TODO: What should we do here? Not complete the request until there's
        // space?
        core_log(CORE_LOG_DEBUG, "input buffer full, dropping input");
        return;
    }

    /* Echo the character. */
    echoInput(ch, false);

    m_inputBuffer[(m_inputBufferStart + m_inputBufferSize) % kInputBufferMax] = ch;

    m_inputBufferSize++;
    if (ch & kChar_NewLine)
        m_inputBufferLines++;

    /* Check if we have any pending reads which can now be completed. */
    for (auto it = m_pendingReads.begin(); it != m_pendingReads.end(); ) {
        if (readBuffer(*it)) {
            it = m_pendingReads.erase(it);
        } else {
            ++it;
        }
    }

    /* Signal events that can be satisfied. */
    signalReadEvents();
}

/** Check if a character is a certain control character according to termios. */
bool Terminal::isControlChar(uint16_t ch, int control) const {
    if (ch & kChar_Escaped || ch == _POSIX_VDISABLE)
        return false;

    return ch == static_cast<uint16_t>(m_termios.c_cc[control]);
}

void Terminal::echoInput(uint16_t ch, bool raw) {
    uint8_t buf[2] = { static_cast<uint8_t>(ch), 0 };
    size_t size = 1;

    if (!(m_termios.c_lflag & ECHO)) {
        /* Even if ECHO is not set, newlines should be echoed if both ECHONL and
         * ICANON are set. */
        if (buf[0] != '\n' || (m_termios.c_lflag & (ECHONL | ICANON)) != (ECHONL | ICANON))
            return;
    }

    if (!raw && buf[0] < ' ') {
        if (ch & kChar_Escaped || (buf[0] != '\n' && buf[0] != '\r' && buf[0] != '\t')) {
            /* Print it as ^ch. */
            buf[0] = '^';
            buf[1] = '@' + (ch & 0xff);
            size++;
        }
    }

    sendOutput(buf, size);
}

/** Determine if the terminal is readable. */
bool Terminal::isReadable() const {
    return (m_termios.c_lflag & ICANON) ? m_inputBufferLines > 0 : m_inputBufferSize > 0;
}

/** Try to read from the input buffer.
 * @return              Whether the operation was completed. */
bool Terminal::readBuffer(ReadOperation &op) {
    /* Canonical mode reads return at most one line and when a line is available
     * can return less data than requested. Non-blocking reads always complete
     * immediately but we can return less data than requested if it's not
     * available. */
    bool allAvailable = (op.canon) ? m_inputBufferLines > 0 : m_inputBufferSize >= op.size;
    bool canComplete  = op.nonblock || allAvailable;

    if (!canComplete)
        return false;

    ipc_message_t reply = initializeFileReply(USER_FILE_OP_READ, op.serial);
    reply.args[USER_FILE_MESSAGE_ARG_READ_STATUS] = (allAvailable) ? STATUS_SUCCESS : STATUS_WOULD_BLOCK;

    /* Gather the data to return. Canonical mode cannot return anything unless
     * we have a whole line. */
    reply.size = (!op.canon || allAvailable) ? std::min(op.size, m_inputBufferSize) : 0;

    std::unique_ptr<uint8_t[]> data;
    if (reply.size > 0)
        data.reset(new uint8_t[reply.size]);

    size_t bufferStart = m_inputBufferStart;
    size_t bufferSize  = m_inputBufferSize;
    size_t bufferLines = m_inputBufferLines;

    for (size_t i = 0; i < reply.size; i++) {
        uint16_t ch = m_inputBuffer[bufferStart];
        data[i] = static_cast<uint8_t>(ch);

        bufferStart = (bufferStart + 1) % kInputBufferMax;
        bufferSize--;

        if (ch & kChar_NewLine) {
            bufferLines--;

            if (op.canon) {
                /* We return regular newlines but not EOF. */
                if (!(ch & kChar_Eof))
                    i++;

                reply.size = i;
                break;
            }
        }
    }

    status_t ret = kern_connection_send(
        m_userFileConnection, &reply, (reply.size > 0) ? data.get() : nullptr,
        INVALID_HANDLE, -1);
    if (ret == STATUS_SUCCESS) {
        /* Only remove from the buffer if we could complete it. */
        m_inputBufferStart = bufferStart;
        m_inputBufferSize  = bufferSize;
        m_inputBufferLines = bufferLines;
    } else if (ret != STATUS_CANCELLED) {
        core_log(CORE_LOG_WARN, "failed to send file message %" PRIu32 ": %" PRId32, reply.id, ret);
    }

    return true;
}

/** Try to erase a character from the current line of the input buffer.
 * @return              Whether a character was erased. */
bool Terminal::eraseChar() {
    if (m_inputBufferSize == 0)
        return false;

    size_t pos = (m_inputBufferStart + m_inputBufferSize - 1) % kInputBufferMax;

    if (m_inputBuffer[pos] & kChar_NewLine)
        return false;

    m_inputBufferSize--;
    return true;
}

/** Try to erase a line from the input buffer.
 * @return              Number of characters erased. */
size_t Terminal::eraseLine() {
    size_t erased = 0;
    while (eraseChar())
        erased++;

    return erased;
}

/** Discard all unread input. */
void Terminal::clearBuffer() {
    m_inputBufferStart = 0;
    m_inputBufferSize  = 0;
    m_inputBufferLines = 0;
}
