// Protocol Buffers - Google's data interchange format
// Copyright 2014 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "protobuf.h"

VALUE cError;
VALUE cParseError;
VALUE cTypeError;
VALUE c_only_cookie = Qnil;

static VALUE cached_empty_string = Qnil;
static VALUE cached_empty_bytes = Qnil;

static VALUE create_frozen_string(const char* str, size_t size, bool binary) {
  VALUE str_rb = rb_str_new(str, size);

  rb_enc_associate(str_rb,
                   binary ? kRubyString8bitEncoding : kRubyStringUtf8Encoding);
  rb_obj_freeze(str_rb);
  return str_rb;
}

VALUE get_frozen_string(const char* str, size_t size, bool binary) {
  if (size == 0) {
    return binary ? cached_empty_bytes : cached_empty_string;
  } else {
    // It is harder to memoize non-empty strings.  The obvious approach would be
    // to use a Ruby hash keyed by string as memo table, but looking up in such a table
    // requires constructing a string (the very thing we're trying to avoid).
    //
    // Since few fields have defaults, we will just optimize the empty string
    // case for now.
    return create_frozen_string(str, size, binary);
  }
}

// -----------------------------------------------------------------------------
// Utilities.
// -----------------------------------------------------------------------------

// Raises a Ruby error if |status| is not OK, using its error message.
void check_upb_status(const upb_status* status, const char* msg) {
  if (!upb_ok(status)) {
    rb_raise(rb_eRuntimeError, "%s: %s\n", msg, upb_status_errmsg(status));
  }
}

// String encodings: we look these up once, at load time, and then cache them
// here.
rb_encoding* kRubyStringUtf8Encoding;
rb_encoding* kRubyStringASCIIEncoding;
rb_encoding* kRubyString8bitEncoding;

// Ruby-interned string: "descriptor". We use this identifier to store an
// instance variable on message classes we create in order to link them back to
// their descriptors.
//
// We intern this once at module load time then use the interned identifier at
// runtime in order to avoid the cost of repeatedly interning in hot paths.
const char* kDescriptorInstanceVar = "descriptor";
ID descriptor_instancevar_interned;

// -----------------------------------------------------------------------------
// Initialization/entry point.
// -----------------------------------------------------------------------------

// This must be named "Init_protobuf_c" because the Ruby module is named
// "protobuf_c" -- the VM looks for this symbol in our .so.
void Init_protobuf_c() {
  VALUE google = rb_define_module("Google");
  VALUE protobuf = rb_define_module_under(google, "Protobuf");
  VALUE internal = rb_define_module_under(protobuf, "Internal");

  descriptor_instancevar_interned = rb_intern(kDescriptorInstanceVar);
  DescriptorPool_register(protobuf);
  Descriptor_register(protobuf);
  FileDescriptor_register(protobuf);
  FieldDescriptor_register(protobuf);
  OneofDescriptor_register(protobuf);
  EnumDescriptor_register(protobuf);
  MessageBuilderContext_register(internal);
  OneofBuilderContext_register(internal);
  EnumBuilderContext_register(internal);
  FileBuilderContext_register(internal);
  Builder_register(internal);
  RepeatedField_register(protobuf);
  Map_register(protobuf);

  cError = rb_const_get(protobuf, rb_intern("Error"));
  cParseError = rb_const_get(protobuf, rb_intern("ParseError"));
  cTypeError = rb_const_get(protobuf, rb_intern("TypeError"));

  rb_define_singleton_method(protobuf, "discard_unknown",
                             Google_Protobuf_discard_unknown, 1);
  rb_define_singleton_method(protobuf, "deep_copy",
                             Google_Protobuf_deep_copy, 1);

  kRubyStringUtf8Encoding = rb_utf8_encoding();
  kRubyStringASCIIEncoding = rb_usascii_encoding();
  kRubyString8bitEncoding = rb_ascii8bit_encoding();

  rb_gc_register_address(&c_only_cookie);
  c_only_cookie = rb_class_new_instance(0, NULL, rb_cObject);

  rb_gc_register_address(&cached_empty_string);
  rb_gc_register_address(&cached_empty_bytes);
  cached_empty_string = create_frozen_string("", 0, false);
  cached_empty_bytes = create_frozen_string("", 0, true);
}