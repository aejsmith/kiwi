/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Terminal service.
 */

#pragma once

#include <kiwi/core/handle.h>

class TerminalService {
public:
    TerminalService();
    ~TerminalService();

    int run();

private:
    Kiwi::Core::Handle m_port;
    size_t m_nextTerminalId;
};

extern TerminalService g_terminalService;
