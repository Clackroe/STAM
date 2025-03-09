
directory="${PWD}/src"
src_extensions=" -o -name '*.c'"

include_dirs="-I src/"

file_paths=$(find "$directory" -type f \( -name "*.cpp" -o -name "*.c" \) -print)

FLAGS="-Wall -Wextra $include_dirs"

bear -- clang $file_paths $FLAGS -o main
