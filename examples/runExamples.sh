#!/bin/bash

# Unnarize Comprehensive Test Runner & Report Generator
# This script runs all .unna examples, shows progress, and captures detailed output.

REPORT_FILE="examples/verification_report.txt"
BIN="./bin/unnarize"

# Ensure binary exists
if [ ! -f "$BIN" ]; then
    echo "Error: Binary not found at $BIN. Please run 'make' first."
    exit 1
fi

echo "=========================================================="
# shellcheck disable=SC2016
echo '   Unnarize Comprehensive Runner (Examples & Benchmarks)'
echo "=========================================================="
echo "Starting test suite at $(date)"
echo ""

# Detect System Specs
CPU_INFO=$(grep "model name" /proc/cpuinfo | head -n 1 | cut -d ':' -f 2 | xargs)
MEM_TOTAL=$(grep MemTotal /proc/meminfo | awk '{print $2/1024 " MB"}')
OS_INFO=$(cat /etc/os-release | grep PRETTY_NAME | cut -d '"' -f 2)

# Initialize Report
echo "Unnarize Execution & Benchmark Report" > "$REPORT_FILE"
echo "Generated on: $(date)" >> "$REPORT_FILE"
echo "--------------------------------------" >> "$REPORT_FILE"
echo "SYSTEM SPECIFICATIONS:" >> "$REPORT_FILE"
echo "  OS:  $OS_INFO" >> "$REPORT_FILE"
echo "  CPU: $CPU_INFO" >> "$REPORT_FILE"
echo "  RAM: $MEM_TOTAL" >> "$REPORT_FILE"
echo "--------------------------------------" >> "$REPORT_FILE"

# Find and sort all .unna files
FILES=$(find examples -name "*.unna" | sort)
TOTAL=$(echo "$FILES" | wc -l)
COUNT=0
PASSED=0

# Clean up ANSI escape codes from report details for better readability in text file
clean_output() {
    echo "$1" | sed 's/\x1B\[[0-9;]*[JKmsu]//g'
}

TOTAL_CPU=0
TOTAL_RAM=0
PEAK_CPU=0
PEAK_RAM=0
VALID_METRICS=0

extract_metrics() {
    local time_out=$1
    local cpu=$(echo "$time_out" | grep "Percent of CPU this job got" | awk '{print $7}' | tr -d '%')
    local ram=$(echo "$time_out" | grep "Maximum resident set size" | awk '{print $6}')
    
    # Defaults if not found
    if [ -z "$cpu" ]; then cpu=0; fi
    if [ -z "$ram" ]; then ram=0; fi
    
    echo "$cpu $ram"
}


for f in $FILES; do
    COUNT=$((COUNT + 1))
    echo -n "[$COUNT/$TOTAL] Running $f..."
    
    # Run with 10s timeout and capture stdout/stderr plus time metrics
    # We use a temporary file for time output because time writes to stderr
    time_file=$(mktemp)
    temp_out=$(/usr/bin/time -v -o "$time_file" timeout 10s "$BIN" "$f" 2>&1)
    exit_code=$?
    
    # Extract metrics
    time_out=$(cat "$time_file")
    rm -f "$time_file"
    
    metrics=$(extract_metrics "$time_out")
    cpu_usage=$(echo "$metrics" | awk '{print $1}')
    ram_usage=$(echo "$metrics" | awk '{print $2}')
    
    # Track Peaks
    if (( $(echo "$cpu_usage > $PEAK_CPU" | bc -l) )); then PEAK_CPU=$cpu_usage; fi
    if [ "$ram_usage" -gt "$PEAK_RAM" ]; then PEAK_RAM=$ram_usage; fi
    
    # Accumulate valid metrics
    if [ "$ram_usage" -gt "0" ]; then
        TOTAL_CPU=$(echo "$TOTAL_CPU + $cpu_usage" | bc -l)
        TOTAL_RAM=$((TOTAL_RAM + ram_usage))
        VALID_METRICS=$((VALID_METRICS + 1))
    fi
    
    status_str=""
    if [ $exit_code -eq 0 ]; then
        echo -e "\033[0;32m PASS \033[0m (CPU: ${cpu_usage}%, RAM: ${ram_usage}KB)"
        PASSED=$((PASSED + 1))
        echo "x] $f: PASS (CPU: ${cpu_usage}%, RAM: ${ram_usage}KB)" >> "$REPORT_FILE"
        echo "    Output Details:" >> "$REPORT_FILE"
        clean_output "$temp_out" | sed 's/^/      /' >> "$REPORT_FILE"
    elif [ $exit_code -eq 124 ]; then
        # Check if the file is a known long-running example (TUI, API Server, Scraper)
        if [[ "$f" == *"tui"* || "$f" == *"advanced_demo"* || "$f" == *"api_server"* || "$f" == *"corelib/scraper/"* ]]; then
            echo -e "\033[0;32m PASS (Timeout Expected) \033[0m (CPU: ${cpu_usage}%, RAM: ${ram_usage}KB)"
            PASSED=$((PASSED + 1))
            echo "x] $f: PASS (Timeout Expected) (CPU: ${cpu_usage}%, RAM: ${ram_usage}KB)" >> "$REPORT_FILE"
            echo "    Details: Service ran successfully until timeout" >> "$REPORT_FILE"
        else
            echo -e "\033[0;31m TIMEOUT \033[0m (CPU: ${cpu_usage}%, RAM: ${ram_usage}KB)"
            echo " ] $f: FAIL (Timeout) (CPU: ${cpu_usage}%, RAM: ${ram_usage}KB)" >> "$REPORT_FILE"
            echo "    Error Details: Execution timed out after 10s" >> "$REPORT_FILE"
        fi
    else
        echo -e "\033[0;31m FAIL \033[0m (CPU: ${cpu_usage}%, RAM: ${ram_usage}KB)"
        echo " ] $f: FAIL (Exit Code: $exit_code) (CPU: ${cpu_usage}%, RAM: ${ram_usage}KB)" >> "$REPORT_FILE"
        echo "    Error Details:" >> "$REPORT_FILE"
        clean_output "$temp_out" | sed 's/^/      /' >> "$REPORT_FILE"
    fi
    
    echo "--------------------------------------" >> "$REPORT_FILE"
done

# Calculate Averages safely
AVG_CPU=0
AVG_RAM=0
if [ "$VALID_METRICS" -gt "0" ]; then
    AVG_CPU=$(echo "scale=2; $TOTAL_CPU / $VALID_METRICS" | bc -l)
    AVG_RAM=$((TOTAL_RAM / VALID_METRICS))
fi

echo ""
echo "=========================================================="
echo "RESULTS SUMMARY:"
echo "Total Tests: $TOTAL"
echo "Passed:      $PASSED"
echo "Failed:      $((TOTAL - PASSED))"
echo "Avg CPU:     ${AVG_CPU}%"
echo "Peak CPU:    ${PEAK_CPU}%"
echo "Avg RAM:     ${AVG_RAM} KB"
echo "Peak RAM:    ${PEAK_RAM} KB"
echo ""
echo "Detailed report saved to: $REPORT_FILE"
echo "=========================================================="

# Append to file
echo "" >> "$REPORT_FILE"
echo "==========================================================" >> "$REPORT_FILE"
echo "RESULTS SUMMARY:" >> "$REPORT_FILE"
echo "Total Tests: $TOTAL" >> "$REPORT_FILE"
echo "Passed:      $PASSED" >> "$REPORT_FILE"
echo "Failed:      $((TOTAL - PASSED))" >> "$REPORT_FILE"
echo "Avg CPU:     ${AVG_CPU}%" >> "$REPORT_FILE"
echo "Peak CPU:    ${PEAK_CPU}%" >> "$REPORT_FILE"
echo "Avg RAM:     ${AVG_RAM} KB" >> "$REPORT_FILE"
echo "Peak RAM:    ${PEAK_RAM} KB" >> "$REPORT_FILE"
echo "==========================================================" >> "$REPORT_FILE"
