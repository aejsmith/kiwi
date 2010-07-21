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
 * @brief		RTLD main function.
 */

#include <kernel/errors.h>
#include <kernel/process.h>

#include "args.h"
#include "image.h"
#include "utility.h"

extern void *rtld_main(process_args_t *args);

/** Pointer to application image structure. */
rtld_image_t *rtld_application = NULL;

/** Main function for the RTLD.
 * @todo		Unload all libraries when returning failure so that
 *			FINI functions get called.
 * @param args		Arguments structure provided by the kernel.
 * @return		Program entry point address. */
void *rtld_main(process_args_t *args) {
	rtld_image_t *image;
	void (*func)(void);
	void *entry;
	int ret;

	rtld_args_init(args);

	/* Check that we're not attempting to load ourselves. */
	if(strcmp(args->path, "/system/binaries/rtld-" CONFIG_ARCH) == 0) {
		printf("RTLD: Should not be invoked directly!\n");
		ret = -ERR_PARAM_INVAL;
		goto fail;
	}

	/* Load the binary and all its dependencies. */
	dprintf("RTLD: Loading binary: %s\n", args->path);
	if((ret = rtld_image_load(args->path, NULL, ELF_ET_EXEC, &entry, &rtld_application)) != 0) {
		dprintf("RTLD: Failed to load binary (%d)\n", ret);
		goto fail;
	}

	/* Print out some debugging information. */
	dprintf("RTLD: Final image list:\n");
	if(rtld_debug || rtld_dryrun) {
		LIST_FOREACH(&rtld_loaded_images, iter) {
			image = list_entry(iter, rtld_image_t, header);

			if(image->path) {
				printf("  %s => %s (%p)\n", image->name, image->path, image->load_base);
			} else {
				printf("  %s (%p)\n", image->name, image->load_base);
			}
		}
	}

	/* If doing a dry run all we want to know is the final list, don't want
	 * to actually run the program. */
	if(rtld_dryrun) {
		process_exit(0);
	}

	/* Call INIT functions for loaded images. */
	LIST_FOREACH(&rtld_loaded_images, iter) {
		image = list_entry(iter, rtld_image_t, header);

		if(image->dynamic[ELF_DT_INIT]) {
			func = (void (*)(void))(image->load_base + image->dynamic[ELF_DT_INIT]);
			dprintf("RTLD: Calling INIT function %p... (%s)\n", func, image->name);
			func();
		}
	}

	/* Return the program entry point for the startup code to call. */
	dprintf("RTLD: Calling entry point %p...\n", entry);
	return entry;
fail:
	while(!list_empty(&rtld_loaded_images)) {
		image = list_entry(rtld_loaded_images.next, rtld_image_t, header);
		rtld_image_unload(image);
	}

	/* This will cause the process_create() call to return our error code. */
	process_exit(ret);
}
