#pragma once

#include <Zydis/Mnemonic.h>
#include <Zydis/Register.h>

#include <string>

namespace Mnemonic {

    static ZydisMnemonic FromString(const std::string& str)
    {
        for (size_t i = 0; i <= ZYDIS_MNEMONIC_MAX_VALUE; i++) {
            const char* mnemonicStr = ZydisMnemonicGetString(static_cast<ZydisMnemonic>(i));
            if (mnemonicStr && str == mnemonicStr) {
                return static_cast<ZydisMnemonic>(i);
            }
        }
        return ZYDIS_MNEMONIC_INVALID;
    }

} // namespace Mnemonic

namespace Register {

    static ZydisRegister FromString(const std::string& str)
    {
        for (int i = 0; i <= ZYDIS_REGISTER_MAX_VALUE; i++) {
            const char* regStr = ZydisRegisterGetString(static_cast<ZydisRegister>(i));
            if (regStr && str == regStr) {
                return static_cast<ZydisRegister>(i);
            }
        }
        return ZYDIS_REGISTER_NONE;
    }

} // namespace Register

namespace BitSize {

    static zasm::BitSize FromString(const std::string& str)
    {
        if (str == "byte") {
            return zasm::BitSize::_8;
        } else if (str == "word") {
            return zasm::BitSize::_16;
        } else if (str == "dword") {
            return zasm::BitSize::_32;
        } else if (str == "qword") {
            return zasm::BitSize::_64;
        } else if (str == "tword") {
            return zasm::BitSize::_80;
        } else if (str == "xmmword") {
            return zasm::BitSize::_128;
        } else if (str == "ymmword") {
            return zasm::BitSize::_256;
        } else if (str == "zmmword") {
            return zasm::BitSize::_512;
        }
        return zasm::BitSize::_0;
    }

} // namespace BitSize