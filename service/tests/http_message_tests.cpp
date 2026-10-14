// Governed by docs/adr/ADR-0003-local-runtime-path.md.
//
// Unit suite for the orange HTTP wire-format layer. The harness is
// hand-rolled on purpose: the default dependency set is the C++ standard
// library and POSIX, and a third-party test framework would require an
// accepted ADR (docs/development/cpp-development-standard.md, Dependencies).
// CTest judges this executable by its process exit status; CheckReporter
// turns each failed check into a stderr line, and main() turns the failure
// count into that exit status.

#include <cstdio>
#include <format>
#include <source_location>
#include <string_view>

namespace {

// Records check failures for one run and renders each with its source
// location; the process exit status derived here is what CTest sees.
class CheckReporter {
  public:
    // Evaluates one named expectation. `location` defaults to the call site
    // (a C++20 idiom), so callers write plain descriptions and still get
    // exact file:line data in every failure report.
    void check(bool condition, std::string_view description,
               std::source_location location = std::source_location::current()) {
        if (condition) {
            return;
        }
        ++failures_;
        std::fputs(std::format("{}:{}: CHECK failed: {}\n", location.file_name(), location.line(),
                               description)
                       .c_str(),
                   stderr);
    }

    // Maps the failure count onto the process exit status: 0 passes, 1 fails.
    [[nodiscard]] int exit_status() const { return failures_ == 0 ? 0 : 1; }

  private:
    int failures_ = 0;
};

} // namespace

int main() {
    CheckReporter reporter;

    // Wiring sanity: proves the executable runs and that a passing check
    // yields exit status 0 under CTest. The development sequence first
    // observed a deliberately failing check; the evidence is recorded with
    // this change's verification runs.
    reporter.check(1 + 1 == 2, "harness wiring: arithmetic sanity");

    return reporter.exit_status();
}
