/*
 * Copyright (c) 2018-2023, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, the SerenityOS developers.
 * Copyright (c) 2021, Sam Atkins <atkinssj@serenityos.org>
 * Copyright (c) 2023, Srikavin Ramkumar <me@srikavin.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/DOM/DocumentLoadEventDelayer.h>
#include <LibWeb/Fetch/Infrastructure/FetchAlgorithms.h>
#include <LibWeb/Fetch/Infrastructure/HTTP/Requests.h>
#include <LibWeb/HTML/CORSSettingAttribute.h>
#include <LibWeb/HTML/HTMLElement.h>
#include <LibWeb/Loader/Resource.h>

namespace Web::HTML {

class HTMLLinkElement final
    : public HTMLElement
    , public ResourceClient {
    WEB_PLATFORM_OBJECT(HTMLLinkElement, HTMLElement);

public:
    virtual ~HTMLLinkElement() override;

    virtual void inserted() override;

    DeprecatedString rel() const { return attribute(HTML::AttributeNames::rel); }
    DeprecatedString type() const { return attribute(HTML::AttributeNames::type); }
    DeprecatedString href() const { return attribute(HTML::AttributeNames::href); }

    bool has_loaded_icon() const;
    bool load_favicon_and_use_if_window_is_active();

private:
    HTMLLinkElement(DOM::Document&, DOM::QualifiedName);

    virtual JS::ThrowCompletionOr<void> initialize(JS::Realm&) override;
    void parse_attribute(DeprecatedFlyString const&, DeprecatedString const&) override;

    // ^ResourceClient
    virtual void resource_did_fail() override;
    virtual void resource_did_load() override;

    // ^ HTMLElement
    virtual void did_remove_attribute(DeprecatedFlyString const&) override;
    virtual void visit_edges(Cell::Visitor&) override;

    struct LinkProcessingOptions {
        // href (default the empty string)
        String href {};
        // destination (default the empty string)
        Optional<Fetch::Infrastructure::Request::Destination> destination {};
        // initiator (default "link")
        Optional<Fetch::Infrastructure::Request::InitiatorType> initiator { Fetch::Infrastructure::Request::InitiatorType::Link };
        // integrity (default the empty string)
        String integrity {};
        // type (default the empty string)
        String type {};
        // cryptographic nonce metadata (default the empty string)
        //     A string
        String cryptographic_nonce_metadata {};
        // crossorigin (default No CORS)
        //     A CORS settings attribute state
        CORSSettingAttribute crossorigin { CORSSettingAttribute::NoCORS };
        // referrer policy (default the empty string)
        //      A referrer policy
        Optional<ReferrerPolicy::ReferrerPolicy> referrer_policy {};
        // FIXME: source set (default null)
        //          Null or a source set
        // base URL
        //      A URL
        AK::URL base_url;
        // origin
        //      An origin
        HTML::Origin origin;
        // environment
        //      An environment
        JS::GCPtr<HTML::EnvironmentSettingsObject> environment;
        // policy container
        //      A policy container
        HTML::PolicyContainer policy_container;
        // document (default null)
        //      Null or a Document
        JS::GCPtr<Web::DOM::Document> document;
        // FIXME: on document ready (default null)
        //          Null or an algorithm accepting a Document
    };

    // https://html.spec.whatwg.org/multipage/semantics.html#create-link-options-from-element
    LinkProcessingOptions create_link_options();

    // https://html.spec.whatwg.org/multipage/semantics.html#create-a-link-request
    JS::GCPtr<Fetch::Infrastructure::Request> create_link_request(LinkProcessingOptions const&);

    // https://html.spec.whatwg.org/multipage/semantics.html#linked-resource-fetch-setup-steps
    bool linked_resource_fetch_setup_steps(Fetch::Infrastructure::Request&);

    // https://html.spec.whatwg.org/multipage/links.html#link-type-stylesheet:linked-resource-fetch-setup-steps
    bool stylesheet_linked_resource_fetch_setup_steps(Fetch::Infrastructure::Request&);

    // https://html.spec.whatwg.org/multipage/semantics.html#fetch-and-process-the-linked-resource
    void fetch_and_process_linked_resource();

    // https://html.spec.whatwg.org/multipage/semantics.html#process-the-linked-resource
    void process_linked_resource(bool success, Fetch::Infrastructure::Response const&, Variant<Empty, Fetch::Infrastructure::FetchAlgorithms::ConsumeBodyFailureTag, ByteBuffer>);

    // https://html.spec.whatwg.org/multipage/links.html#link-type-stylesheet:process-the-linked-resource
    void process_stylesheet_resource(bool success, Fetch::Infrastructure::Response const&, Variant<Empty, Fetch::Infrastructure::FetchAlgorithms::ConsumeBodyFailureTag, ByteBuffer>);

    // https://html.spec.whatwg.org/multipage/semantics.html#default-fetch-and-process-the-linked-resource
    void default_fetch_and_process_linked_resource();

    void resource_did_load_favicon();

    struct Relationship {
        enum {
            Alternate = 1 << 0,
            Stylesheet = 1 << 1,
            Preload = 1 << 2,
            DNSPrefetch = 1 << 3,
            Preconnect = 1 << 4,
            Icon = 1 << 5,
        };
    };

    JS::GCPtr<CSS::CSSStyleSheet> m_loaded_style_sheet;

    Optional<DOM::DocumentLoadEventDelayer> m_document_load_event_delayer;
    unsigned m_relationship { 0 };
};

}
