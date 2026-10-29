# ADR: docs/adr/ADR-0008-http-parser-benchmarks.md
# ADR: service/docs/adr/ADR-0004-owned-request-storage.md
# Authorization: docs/reviews/2026-10-16-http-performance.md.
# Executes the benchmark's public CLI and validates its machine-readable report.
execute_process(COMMAND "${BENCHMARK}" --quick RESULT_VARIABLE status
                OUTPUT_VARIABLE report ERROR_VARIABLE diagnostic)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "benchmark execution failed: ${status}: ${diagnostic}")
endif()
string(JSON count LENGTH "${report}" cases)
if(NOT count EQUAL 18)
    message(FATAL_ERROR "expected eighteen benchmark fixtures")
endif()
math(EXPR last "${count} - 1")
foreach(index RANGE ${last})
    foreach(field IN ITEMS iterations repetitions median_ns min_ns max_ns wire_bytes)
        string(JSON value GET "${report}" cases ${index} ${field})
        if(NOT value GREATER 0)
            message(FATAL_ERROR "${field} must be positive")
        endif()
    endforeach()
endforeach()
# The separate counting executable must retain every fixture and its real result.
execute_process(COMMAND "${ALLOCATIONS}" RESULT_VARIABLE allocation_status
                OUTPUT_VARIABLE allocation_report ERROR_VARIABLE allocation_diagnostic)
if(NOT allocation_status STREQUAL "0")
    message(FATAL_ERROR "allocation measurement failed: ${allocation_diagnostic}")
endif()
string(JSON allocation_count LENGTH "${allocation_report}" cases)
if(NOT allocation_count EQUAL count)
    message(FATAL_ERROR "timing and allocation fixture counts disagree")
endif()
foreach(index RANGE ${last})
    string(JSON timing_name GET "${report}" cases ${index} name)
    string(JSON allocation_name GET "${allocation_report}" cases ${index} name)
    string(JSON calls GET "${allocation_report}" cases ${index} calls)
    string(JSON checksum GET "${allocation_report}" cases ${index} checksum)
    if(NOT timing_name STREQUAL allocation_name OR calls LESS 0 OR NOT checksum GREATER 0)
        message(FATAL_ERROR "allocation report lost a fixture or its observed result")
    endif()
endforeach()
execute_process(COMMAND "${BENCHMARK}" --unknown RESULT_VARIABLE invalid
                OUTPUT_QUIET ERROR_QUIET)
if(NOT invalid STREQUAL "2")
    message(FATAL_ERROR "invalid CLI arguments must exit 2")
endif()
# A near-zero scale forces the real smoke gate to reject measured latency.
execute_process(COMMAND "${BENCHMARK}" --quick --smoke-scale 0.000001
                RESULT_VARIABLE slow OUTPUT_QUIET ERROR_QUIET)
if(NOT slow STREQUAL "1")
    message(FATAL_ERROR "exceeded smoke limits must exit 1")
endif()
