#include "./auth_abstract.hpp"

std::string CombineAccountPass(std::string_view AccountView, std::string_view PassView) {
    return std::string(AccountView) + '\x00' + std::string(PassView);
}
