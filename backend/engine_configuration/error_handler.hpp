#include <exception>

class MECException : public std::exception {
public:
    const char* what() const noexcept override {
        return "MEC encountered an internal error";
    }
};
