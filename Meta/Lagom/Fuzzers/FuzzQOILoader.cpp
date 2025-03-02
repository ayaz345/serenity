/*
 * Copyright (c) 2020, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGfx/ImageFormats/QOILoader.h>
#include <stddef.h>
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size)
{
    auto decoder_or_error = Gfx::QOIImageDecoderPlugin::create({ data, size });
    if (decoder_or_error.is_error())
        return 0;
    auto decoder = decoder_or_error.release_value();
    if (!decoder->initialize().is_error()) {
        (void)decoder->frame(0);
    }
    return 0;
}
