// syscalls.cpp — Implementación de los servicios SPIM más usados.
#include "syscalls.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <string>

namespace rv {

// ---------------------------------------------------------------- Console --
void Console::write(const std::string& s) {
    if (s.empty()) return;
    out_ << s << std::flush;
    captured_ += s;
    atLineStart_ = (s.back() == '\n');
}

void Console::ensureNewline() {
    if (!atLineStart_) {
        out_ << '\n' << std::flush;
        atLineStart_ = true;
    }
}

// ----------------------------------------------------------- SpimSyscalls --
bool SpimSyscalls::readLine(std::string& line) {
    if (!queued_.empty()) {
        line = queued_.front();
        queued_.pop_front();
        console_.write(line + "\n");  // se muestra, como si se hubiera tecleado
        return true;
    }
    if (!std::getline(in_, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF (Windows)
    if (echoInput_) console_.write(line + "\n");
    else console_.markLineStart();  // el Enter del usuario ya bajó de línea
    return true;
}

const char* SpimSyscalls::serviceName(uint32_t service) {
    switch (service) {
        case 1: return "print_int";     case 4: return "print_string";
        case 5: return "read_int";      case 8: return "read_string";
        case 9: return "sbrk";          case 10: return "exit";
        case 11: return "print_char";   case 12: return "read_char";
        case 17: return "exit2";        case 30: return "time";
        case 32: return "sleep";        case 34: return "print_hex";
        case 35: return "print_bin";    case 36: return "print_uint";
        case 93: return "exit";         default: return "desconocido";
    }
}

SyscallHandler::Result SpimSyscalls::handle(Cpu& cpu) {
    const uint32_t service = cpu.reg(17);  // a7
    const uint32_t a0 = cpu.reg(10);
    const uint32_t a1 = cpu.reg(11);
    Result res;
    char buf[64];

    switch (service) {
        case 1:  // print integer (con signo)
            console_.write(std::to_string(static_cast<int32_t>(a0)));
            break;

        case 4: {  // print string (terminada en '\0')
            std::string s;
            for (uint32_t addr = a0; s.size() < (1u << 20); ++addr) {
                const char c = static_cast<char>(cpu.mem.read8(addr));
                if (c == '\0') break;
                s += c;
            }
            console_.write(s);
            break;
        }

        case 5: {  // read integer
            std::string line;
            if (!readLine(line)) return {Result::Error, 0, "read_int: no hay más entrada (EOF)"};
            char* end = nullptr;
            const long long v = std::strtoll(line.c_str(), &end, 10);
            cpu.setReg(10, static_cast<uint32_t>(v));
            break;
        }

        case 8: {  // read string: a0 = buffer, a1 = tamaño máximo (incluye '\0')
            std::string line;
            if (!readLine(line)) return {Result::Error, 0, "read_string: no hay más entrada (EOF)"};
            if (a1 == 0) break;
            line += '\n';                    // como SPIM: se guarda el salto de línea
            const uint32_t room = a1 - 1;    // deja espacio para el '\0'
            uint32_t i = 0;
            for (; i < room && i < line.size(); ++i)
                cpu.mem.write8(a0 + i, static_cast<uint8_t>(line[i]));
            cpu.mem.write8(a0 + i, 0);
            break;
        }

        case 9: {  // sbrk: reserva a0 bytes y devuelve el puntero en a0
            const uint32_t ptr = heap_;
            heap_ = (heap_ + a0 + 3u) & ~3u;
            cpu.setReg(10, ptr);
            break;
        }

        case 10:  // exit
            res.kind = Result::Exit;
            res.code = 0;
            break;

        case 11:  // print character
            console_.write(std::string(1, static_cast<char>(a0 & 0xFF)));
            break;

        case 12: {  // read character
            if (!queued_.empty()) {
                std::string& q = queued_.front();
                const char ch = q.empty() ? '\n' : q[0];
                if (q.empty()) queued_.pop_front(); else q.erase(0, 1);
                if (!queued_.empty() && queued_.front().empty()) queued_.pop_front();
                console_.write(std::string(1, ch) + "\n");
                cpu.setReg(10, static_cast<uint32_t>(static_cast<unsigned char>(ch)));
                break;
            }
            const int c = in_.get();
            if (c == EOF) return {Result::Error, 0, "read_char: no hay más entrada (EOF)"};
            if (echoInput_) console_.write(std::string(1, static_cast<char>(c)) + "\n");
            else console_.markLineStart();
            // Consume el Enter que sigue, para que no se interprete como comando.
            if (c != '\n' && in_.peek() == '\r') in_.get();
            if (c != '\n' && in_.peek() == '\n') in_.get();
            cpu.setReg(10, static_cast<uint32_t>(c & 0xFF));
            break;
        }

        case 17:  // exit2 (código en a0)
        case 93:  // exit (convención de Linux, la usan programas compilados en C)
            res.kind = Result::Exit;
            res.code = static_cast<int32_t>(a0);
            break;

        case 30: {  // tiempo en ms desde 1970 -> a0 (bajo), a1 (alto)
            using namespace std::chrono;
            const uint64_t ms = static_cast<uint64_t>(
                duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
            cpu.setReg(10, static_cast<uint32_t>(ms));
            cpu.setReg(11, static_cast<uint32_t>(ms >> 32));
            break;
        }

        case 32: {  // sleep a0 ms (espera activa, portable)
            using namespace std::chrono;
            const auto until = steady_clock::now() + milliseconds(a0 > 10000 ? 10000 : a0);
            while (steady_clock::now() < until) {}
            break;
        }

        case 34:  // print integer en hexadecimal
            std::snprintf(buf, sizeof buf, "0x%08x", a0);
            console_.write(buf);
            break;

        case 35: {  // print integer en binario (32 bits)
            std::string s;
            for (int i = 31; i >= 0; --i) s += ((a0 >> i) & 1) ? '1' : '0';
            console_.write(s);
            break;
        }

        case 36:  // print integer sin signo
            console_.write(std::to_string(a0));
            break;

        default:
            (void)a1;
            return {Result::Error, 0, "servicio de syscall no soportado (a7 = " +
                                      std::to_string(service) + ")"};
    }
    return res;
}

}  // namespace rv
