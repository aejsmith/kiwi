/*
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Version information header.
 */

#ifndef __VERSION_H
#define __VERSION_H

/** Version information for the kernel, defined in a build-generated file. */
extern int kiwi_ver_release;		/**< Kiwi release number. */
extern int kiwi_ver_update;		/**< Release update number. */
extern int kiwi_ver_revision;		/**< Release revision number. */
extern const char *kiwi_ver_codename;	/**< Release codename. */
extern const char *kiwi_ver_string;	/**< String of version number. */

#endif /* __VERSION_H */
