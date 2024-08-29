// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google Inc.  All rights reserved.
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

#ifndef GOOGLE_PROTOBUF_DESCRIPTOR_VISITOR_H__
#define GOOGLE_PROTOBUF_DESCRIPTOR_VISITOR_H__

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/generated_message_reflection.h"

namespace google {
namespace protobuf {
namespace internal {

// Visit every node in the descriptors calling `visitor(node, proto)`.
// The visitor does not need to handle all possible node types. Types that are
// not visitable via `visitor` will be ignored.
template <typename Visitor>
void VisitDescriptors(const FileDescriptor& file,
                      const FileDescriptorProto& proto, Visitor visitor);

template <typename Visitor>
void VisitDescriptors(const FileDescriptor& file, FileDescriptorProto& proto,
                      Visitor visitor);

// Visit just the descriptors, without a corresponding proto tree.
template <typename Visitor>
void VisitDescriptors(const FileDescriptor& file, Visitor visitor);

template <typename Visitor>
struct VisitImpl {
  Visitor visitor;
  template <typename... Proto>
  void Visit(const FieldDescriptor& descriptor, Proto&... proto) {
    visitor(descriptor, proto...);
  }

  template <typename... Proto>
  void Visit(const EnumValueDescriptor& descriptor, Proto&... proto) {
    visitor(descriptor, proto...);
  }

  template <typename... Proto>
  void Visit(const EnumDescriptor& descriptor, Proto&... proto) {
    visitor(descriptor, proto...);
    for (int i = 0; i < descriptor.value_count(); i++) {
      Visit(*descriptor.value(i), value(proto, i)...);
    }
  }

  template <typename... Proto>
  void Visit(const Descriptor::ExtensionRange& descriptor, Proto&... proto) {
    visitor(descriptor, proto...);
  }

  template <typename... Proto>
  void Visit(const OneofDescriptor& descriptor, Proto&... proto) {
    visitor(descriptor, proto...);
  }

  template <typename... Proto>
  void Visit(const Descriptor& descriptor, Proto&... proto) {
    visitor(descriptor, proto...);

    for (int i = 0; i < descriptor.enum_type_count(); i++) {
      Visit(*descriptor.enum_type(i), enum_type(proto, i)...);
    }

    for (int i = 0; i < descriptor.oneof_decl_count(); i++) {
      Visit(*descriptor.oneof_decl(i), oneof_decl(proto, i)...);
    }

    for (int i = 0; i < descriptor.field_count(); i++) {
      Visit(*descriptor.field(i), field(proto, i)...);
    }

    for (int i = 0; i < descriptor.nested_type_count(); i++) {
      Visit(*descriptor.nested_type(i), nested_type(proto, i)...);
    }

    for (int i = 0; i < descriptor.extension_count(); i++) {
      Visit(*descriptor.extension(i), extension(proto, i)...);
    }

    for (int i = 0; i < descriptor.extension_range_count(); i++) {
      Visit(*descriptor.extension_range(i), extension_range(proto, i)...);
    }
  }

  template <typename... Proto>
  void Visit(const MethodDescriptor& method, Proto&... proto) {
    visitor(method, proto...);
  }

  template <typename... Proto>
  void Visit(const ServiceDescriptor& descriptor, Proto&... proto) {
    visitor(descriptor, proto...);
    for (int i = 0; i < descriptor.method_count(); i++) {
      Visit(*descriptor.method(i), method(proto, i)...);
    }
  }

  template <typename... Proto>
  void Visit(const FileDescriptor& descriptor, Proto&... proto) {
    visitor(descriptor, proto...);
    for (int i = 0; i < descriptor.message_type_count(); i++) {
      Visit(*descriptor.message_type(i), message_type(proto, i)...);
    }
    for (int i = 0; i < descriptor.enum_type_count(); i++) {
      Visit(*descriptor.enum_type(i), enum_type(proto, i)...);
    }
    for (int i = 0; i < descriptor.extension_count(); i++) {
      Visit(*descriptor.extension(i), extension(proto, i)...);
    }
    for (int i = 0; i < descriptor.service_count(); i++) {
      Visit(*descriptor.service(i), service(proto, i)...);
    }
  }

 private:
#define CREATE_NESTED_GETTER(TYPE, NESTED)                                     \
  inline auto& NESTED(TYPE& desc, int i) { return *desc.mutable_##NESTED(i); } \
  inline auto& NESTED(const TYPE& desc, int i) { return desc.NESTED(i); }

  CREATE_NESTED_GETTER(DescriptorProto, enum_type);
  CREATE_NESTED_GETTER(DescriptorProto, extension);
  CREATE_NESTED_GETTER(DescriptorProto, extension_range);
  CREATE_NESTED_GETTER(DescriptorProto, field);
  CREATE_NESTED_GETTER(DescriptorProto, nested_type);
  CREATE_NESTED_GETTER(DescriptorProto, oneof_decl);
  CREATE_NESTED_GETTER(EnumDescriptorProto, value);
  CREATE_NESTED_GETTER(FileDescriptorProto, enum_type);
  CREATE_NESTED_GETTER(FileDescriptorProto, extension);
  CREATE_NESTED_GETTER(FileDescriptorProto, message_type);
  CREATE_NESTED_GETTER(FileDescriptorProto, service);
  CREATE_NESTED_GETTER(ServiceDescriptorProto, method);

#undef CREATE_NESTED_GETTER
};

// Provide a fallback to ignore all the nodes that are not interesting to the
// input visitor.
template <typename Visitor>
struct VisitorImpl : Visitor {
  explicit VisitorImpl(Visitor visitor) : Visitor(visitor) {}

  // Pull in all of the supplied callbacks.
  using Visitor::operator();

  // Honeypots to ignore all inputs that Visitor does not take.
  struct DescriptorEater {
    template <typename T>
    DescriptorEater(T&&) {}  // NOLINT
  };
  void operator()(DescriptorEater, DescriptorEater) const {}
  void operator()(DescriptorEater) const {}
};

template <typename Visitor>
void VisitDescriptors(const FileDescriptor& file,
                      const FileDescriptorProto& proto, Visitor visitor) {
  using VisitorImpl = internal::VisitorImpl<Visitor>;
  internal::VisitImpl<VisitorImpl>{VisitorImpl(visitor)}.Visit(file, proto);
}

template <typename Visitor>
void VisitDescriptors(const FileDescriptor& file, FileDescriptorProto& proto,
                      Visitor visitor) {
  using VisitorImpl = internal::VisitorImpl<Visitor>;
  internal::VisitImpl<VisitorImpl>{VisitorImpl(visitor)}.Visit(file, proto);
}

template <typename Visitor>
void VisitDescriptors(const FileDescriptor& file, Visitor visitor) {
  using VisitorImpl = internal::VisitorImpl<Visitor>;
  internal::VisitImpl<VisitorImpl>{VisitorImpl(visitor)}.Visit(file);
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_DESCRIPTOR_VISITOR_H__
