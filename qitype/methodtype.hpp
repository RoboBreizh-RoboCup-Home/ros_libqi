#pragma once
/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#ifndef _QITYPE_METHODTYPE_HPP_
#define _QITYPE_METHODTYPE_HPP_

#include <boost/function.hpp>
#include <qitype/functiontype.hpp>

namespace qi
{
  class QITYPE_API MethodType: public Type, public CallableType
  {
  public:
    /// Call with all values of the correct type
    virtual void* call(void* method, void* object,
      void** args, unsigned int nargs) = 0;
    /// Convert and call
    virtual GenericValue call(void* method, GenericValue object,
      const std::vector<GenericValue>& args) = 0;
    /// Return 'linearized' signature function type
    virtual FunctionType* toFunctionType() = 0;
  };

  /// Represents a generic member function. Has value semantic.
  class QITYPE_API GenericMethod
  {
  public:
    GenericMethod();
    GenericMethod(const GenericMethod& b);
    GenericMethod& operator = (const GenericMethod& b);
    ~GenericMethod();

    GenericValue call(GenericValue object, const std::vector<GenericValue> args)
    {
      return type->call(value, object, args);
    }
    std::string signature() const { return type->CallableType::signature();}
    std::string sigreturn() const { return type->CallableType::sigreturn();}

    GenericFunction toGenericFunction();
    MethodType* type;
    void* value;
  };

  template<typename T> MethodType* methodTypeOf();

  template<typename M>
  GenericMethod makeGenericMethod(const M& method);
}

#include <qitype/details/methodtype.hxx>

#endif  // _QITYPE_METHODTYPE_HPP_
