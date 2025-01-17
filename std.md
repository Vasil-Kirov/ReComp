
# IO

## Description
Module provides basic input-output functionality, mainly formatted printing

### Functions

#### `print :: fn(str: string, args: ...)`
#### `println :: fn(str: string, args: ...)`
**Description**
print a formatted string to stdout

**Parameters**
- `str` (string): Format string, the % character means to print the next argument's value at that position
- `args` (...): Variable length arguments, they're printed in order for each % character in the format string 

**Returns**
- None


#### `sprint :: fn(str: string, args: ...) -> string`
#### `vsprint :: fn(str: string, args: []Arg) -> string`
**Description**
format a string and return it

**Parameters**
Same as print

**Returns**
- `string`: A string formatted with all the arguments


#### `read_entire_file :: fn(name: string, alloc: *mem.Allocator) -> (string, bool)`
**Description**
reads a file and returns the contents as a string

**Parameters**
- `name` (string): Path to the file
- `alloc` (Allocator): Allocator to use for storing the contents

**Returns**
- `string`: The file contents, or an empty string
- `bool`: If the entire file was successfully read


#### `readln :: fn(alloc: *mem.Allocator) -> string`
**Description**
read a line from stdin

**Parameters**
- `alloc` (Allocator): Allocator to store the contents

**Returns**
- `string`: The contents read from stdin



#### `read :: fn(bytes: int, alloc: *mem.Allocator) -> string`
**Description**
read a bytes from stdin

**Parameters**
- `bytes` (int): Amount of bytes to read
- `alloc` (Allocator): Allocator to store the contents

**Returns**
- `string`: The contents read from stdin

# mem

## Description
Module provides memory related functions

#### `copy :: fn #link="memcpy"(dst: *, src: *, size: int) -> *;`
#### `set  :: fn #link="memset"(dst: *, c: i32, size: int) -> *;`
#### `cmp  :: fn #link="memcmp"(p1: *, p2: *, size: int) -> i32;`
**Description**
Bindings for memcpy, memset, and memcmp


#### `kb :: fn(n: int) -> int`
#### `mb :: fn(n: int) -> int`
#### `gb :: fn(n: int) -> int`
**Description**
Get the number of bytes for the sepcified memory unit (kb, mb, or gb)

**Parameters**
- `n` (int): the number of memory units wanted

**Returns**
- `int`: the number of bytes for n of the memory unit


#### `make_slice :: fn (T: type, count: int, alloc: *Allocator) -> []T 
**Description**
Allocate a memory view with the given allocator, function asserts for a valid allocation

**Parameters**
- `T` (type): Type of the view
- `count` (int): Number of elements of type T in the view
- `alloc` (Allocator): Allocator to use for the allocation

**Returns**
- `[]T`: Memory view of the allocated region


#### `make_type :: fn (T: type, alloc: *Allocator) -> ?*T`
**Description**
Allocates memory for a single instance of T

**Parameters**
- `T` (type): Type of the value to return
- `alloc` (Allocator): Allocator to use for the allocation

**Returns**
- `?*T`: A pointer to the allocation or null


