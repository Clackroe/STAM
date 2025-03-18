

directory="${PWD}/tests"

test_file_paths_c=$(find "$directory" -type f \(  -name "*.c" \) -print)

test_file_paths_cpp=$(find "$directory" -type f \( -name "*.cpp"  \) -print)


FLAGS="-Wextra"


echo "=====BUILDING C====="
# bear -- clang -g -O0 "$test_file_paths_c" "$FLAGS" -o test_c
bear -- clang "$test_file_paths_c" "$FLAGS" -o test_c

echo "=====BUILDING CPP====="
# bear -- clang++ -g -O0 "$test_file_paths_cpp" "$FLAGS" -o test_cpp
bear -- clang++ "$test_file_paths_cpp" "$FLAGS" -o test_cpp
