
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __gnu_xml_transform_CopyOfNode__
#define __gnu_xml_transform_CopyOfNode__

#pragma interface

#include <gnu/xml/transform/TemplateNode.h>
extern "Java"
{
  namespace gnu
  {
    namespace xml
    {
      namespace transform
      {
          class CopyOfNode;
          class Stylesheet;
          class TemplateNode;
      }
      namespace xpath
      {
          class Expr;
      }
    }
  }
  namespace javax
  {
    namespace xml
    {
      namespace namespace
      {
          class QName;
      }
    }
  }
  namespace org
  {
    namespace w3c
    {
      namespace dom
      {
          class Node;
      }
    }
  }
}

class gnu::xml::transform::CopyOfNode : public ::gnu::xml::transform::TemplateNode
{

public: // actually package-private
  CopyOfNode(::gnu::xml::xpath::Expr *);
  ::gnu::xml::transform::TemplateNode * clone(::gnu::xml::transform::Stylesheet *);
  void doApply(::gnu::xml::transform::Stylesheet *, ::javax::xml::namespace::QName *, ::org::w3c::dom::Node *, jint, jint, ::org::w3c::dom::Node *, ::org::w3c::dom::Node *);
public:
  jboolean references(::javax::xml::namespace::QName *);
  ::java::lang::String * toString();
public: // actually package-private
  ::gnu::xml::xpath::Expr * __attribute__((aligned(__alignof__( ::gnu::xml::transform::TemplateNode)))) select;
public:
  static ::java::lang::Class class$;
};

#endif // __gnu_xml_transform_CopyOfNode__
