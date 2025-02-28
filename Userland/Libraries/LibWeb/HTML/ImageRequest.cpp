/*
 * Copyright (c) 2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/HashTable.h>
#include <LibGfx/Bitmap.h>
#include <LibWeb/Fetch/Fetching/Fetching.h>
#include <LibWeb/Fetch/Infrastructure/FetchAlgorithms.h>
#include <LibWeb/Fetch/Infrastructure/FetchController.h>
#include <LibWeb/Fetch/Infrastructure/HTTP/Responses.h>
#include <LibWeb/HTML/AnimatedBitmapDecodedImageData.h>
#include <LibWeb/HTML/DecodedImageData.h>
#include <LibWeb/HTML/ImageRequest.h>
#include <LibWeb/HTML/ListOfAvailableImages.h>
#include <LibWeb/Platform/ImageCodecPlugin.h>
#include <LibWeb/SVG/SVGDecodedImageData.h>

namespace Web::HTML {

static HashTable<ImageRequest*>& shareable_image_requests()
{
    static HashTable<ImageRequest*> requests;
    return requests;
}

ErrorOr<NonnullRefPtr<ImageRequest>> ImageRequest::create(Page& page)
{
    return adopt_nonnull_ref_or_enomem(new (nothrow) ImageRequest(page));
}

ErrorOr<NonnullRefPtr<ImageRequest>> ImageRequest::get_shareable_or_create(Page& page, AK::URL const& url)
{
    for (auto& it : shareable_image_requests()) {
        if (it->current_url() == url)
            return *it;
    }
    auto request = TRY(create(page));
    request->set_current_url(url);
    return request;
}

ImageRequest::ImageRequest(Page& page)
    : m_page(page)
{
    shareable_image_requests().set(this);
}

ImageRequest::~ImageRequest()
{
    shareable_image_requests().remove(this);
}

// https://html.spec.whatwg.org/multipage/images.html#img-available
bool ImageRequest::is_available() const
{
    // When an image request's state is either partially available or completely available, the image request is said to be available.
    return m_state == State::PartiallyAvailable || m_state == State::CompletelyAvailable;
}

ImageRequest::State ImageRequest::state() const
{
    return m_state;
}

void ImageRequest::set_state(State state)
{
    m_state = state;
}

AK::URL const& ImageRequest::current_url() const
{
    return m_current_url;
}

void ImageRequest::set_current_url(AK::URL url)
{
    m_current_url = move(url);
}

// https://html.spec.whatwg.org/multipage/images.html#abort-the-image-request
void abort_the_image_request(JS::Realm& realm, ImageRequest* image_request)
{
    // 1. If image request is null, then return.
    if (!image_request)
        return;

    // 2. Forget image request's image data, if any.
    image_request->set_image_data(nullptr);

    // 3. Abort any instance of the fetching algorithm for image request,
    //    discarding any pending tasks generated by that algorithm.
    if (auto fetch_controller = image_request->fetch_controller())
        fetch_controller->abort(realm, {});

    image_request->set_fetch_controller(nullptr);
}

RefPtr<DecodedImageData const> ImageRequest::image_data() const
{
    return m_image_data;
}

void ImageRequest::set_image_data(RefPtr<DecodedImageData const> data)
{
    m_image_data = move(data);
}

// https://html.spec.whatwg.org/multipage/images.html#prepare-an-image-for-presentation
void ImageRequest::prepare_for_presentation(HTMLImageElement&)
{
    // FIXME: 1. Let exifTagMap be the EXIF tags obtained from req's image data, as defined by the relevant codec. [EXIF]
    // FIXME: 2. Let physicalWidth and physicalHeight be the width and height obtained from req's image data, as defined by the relevant codec.
    // FIXME: 3. Let dimX be the value of exifTagMap's tag 0xA002 (PixelXDimension).
    // FIXME: 4. Let dimY be the value of exifTagMap's tag 0xA003 (PixelYDimension).
    // FIXME: 5. Let resX be the value of exifTagMap's tag 0x011A (XResolution).
    // FIXME: 6. Let resY be the value of exifTagMap's tag 0x011B (YResolution).
    // FIXME: 7. Let resUnit be the value of exifTagMap's tag 0x0128 (ResolutionUnit).
    // FIXME: 8. If either dimX or dimY is not a positive integer, then return.
    // FIXME: 9. If either resX or resY is not a positive floating-point number, then return.
    // FIXME: 10. If resUnit is not equal to 2 (Inch), then return.
    // FIXME: 11. Let widthFromDensity be the value of physicalWidth, multiplied by 72 and divided by resX.
    // FIXME: 12. Let heightFromDensity be the value of physicalHeight, multiplied by 72 and divided by resY.
    // FIXME: 13. If widthFromDensity is not equal to dimX or heightFromDensity is not equal to dimY, then return.
    // FIXME: 14. If req's image data is CORS-cross-origin, then set img's intrinsic dimensions to dimX and dimY, scale img's pixel data accordingly, and return.
    // FIXME: 15. Set req's preferred density-corrected dimensions to a struct with its width set to dimX and its height set to dimY.
    // FIXME: 16. Update req's img element's presentation appropriately.
}

JS::GCPtr<Fetch::Infrastructure::FetchController> ImageRequest::fetch_controller()
{
    return m_fetch_controller.ptr();
}

void ImageRequest::set_fetch_controller(JS::GCPtr<Fetch::Infrastructure::FetchController> fetch_controller)
{
    m_fetch_controller = move(fetch_controller);
}

void ImageRequest::fetch_image(JS::Realm& realm, JS::NonnullGCPtr<Fetch::Infrastructure::Request> request)
{
    Fetch::Infrastructure::FetchAlgorithms::Input fetch_algorithms_input {};
    fetch_algorithms_input.process_response = [this, &realm, request](JS::NonnullGCPtr<Fetch::Infrastructure::Response> response) {
        // FIXME: If the response is CORS cross-origin, we must use its internal response to query any of its data. See:
        //        https://github.com/whatwg/html/issues/9355
        response = response->unsafe_response();

        // 26. As soon as possible, jump to the first applicable entry from the following list:

        // FIXME: - If the resource type is multipart/x-mixed-replace

        // - If the resource type and data corresponds to a supported image format, as described below
        // - The next task that is queued by the networking task source while the image is being fetched must run the following steps:
        auto process_body = [this, request, response](ByteBuffer data) {
            auto extracted_mime_type = response->header_list()->extract_mime_type().release_value_but_fixme_should_propagate_errors();
            auto mime_type = extracted_mime_type.has_value() ? extracted_mime_type.value().essence().bytes_as_string_view() : StringView {};
            handle_successful_fetch(request->url(), mime_type, move(data));
        };
        auto process_body_error = [this](auto) {
            handle_failed_fetch();
        };

        if (response->body().has_value())
            response->body().value().fully_read(realm, move(process_body), move(process_body_error), JS::NonnullGCPtr { realm.global_object() }).release_value_but_fixme_should_propagate_errors();
    };

    // 25. Fetch the image: Fetch request.
    //     Return from this algorithm, and run the remaining steps as part of the fetch's processResponse for the response response.
    auto fetch_controller = Fetch::Fetching::fetch(
        realm,
        request,
        Fetch::Infrastructure::FetchAlgorithms::create(realm.vm(), move(fetch_algorithms_input)))
                                .release_value_but_fixme_should_propagate_errors();

    set_fetch_controller(fetch_controller);
}

void ImageRequest::add_callbacks(JS::SafeFunction<void()> on_finish, JS::SafeFunction<void()> on_fail)
{
    if (is_available()) {
        if (on_finish)
            on_finish();
        return;
    }

    if (state() == ImageRequest::State::Broken) {
        if (on_fail)
            on_fail();
        return;
    }

    m_callbacks.append({ move(on_finish), move(on_fail) });
}

void ImageRequest::handle_successful_fetch(AK::URL const& url_string, StringView mime_type, ByteBuffer data)
{
    // AD-HOC: At this point, things gets very ad-hoc.
    // FIXME: Bring this closer to spec.

    bool const is_svg_image = mime_type == "image/svg+xml"sv || url_string.basename().ends_with(".svg"sv);

    RefPtr<DecodedImageData> image_data;

    auto handle_failed_decode = [&] {
        for (auto& callback : m_callbacks) {
            if (callback.on_fail)
                callback.on_fail();
        }
    };

    if (is_svg_image) {
        auto result = SVG::SVGDecodedImageData::create(m_page, url_string, data);
        if (result.is_error())
            return handle_failed_decode();

        image_data = result.release_value();
    } else {
        auto result = Web::Platform::ImageCodecPlugin::the().decode_image(data.bytes());
        if (!result.has_value())
            return handle_failed_decode();

        Vector<AnimatedBitmapDecodedImageData::Frame> frames;
        for (auto& frame : result.value().frames) {
            frames.append(AnimatedBitmapDecodedImageData::Frame {
                .bitmap = frame.bitmap,
                .duration = static_cast<int>(frame.duration),
            });
        }
        image_data = AnimatedBitmapDecodedImageData::create(move(frames), result.value().loop_count, result.value().is_animated).release_value_but_fixme_should_propagate_errors();
    }

    set_image_data(image_data);

    // 2. Set image request to the completely available state.
    set_state(ImageRequest::State::CompletelyAvailable);

    for (auto& callback : m_callbacks) {
        if (callback.on_finish)
            callback.on_finish();
    }
}

void ImageRequest::handle_failed_fetch()
{
    for (auto& callback : m_callbacks) {
        if (callback.on_fail)
            callback.on_fail();
    }
}

}
