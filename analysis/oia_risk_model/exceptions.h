#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <string>
#include <iostream>

namespace oia_risk_model{

  // Basic wrapper of std::exception...
  class Exception : std::exception{
  private:
      std::string e;
  public:
      Exception(const std::string s) : e(s){
        std::cout << "ERROR: " << e << "\n";
        throw;
      }
  private:
    virtual const char* what(void) {
      return e.c_str();
    }
  };

} // oia_risk_model

#endif //EXCEPTIONS_H
