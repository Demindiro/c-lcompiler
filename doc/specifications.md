# Specifications

## Types

### Built-in types

**Signed**
- `byte`
- `short`
- `int`
- `long`

**Unsigned**
- `ubyte`
- `ushort`
- `uint`
- `ulong`

**Other**
- `bool`
- `string`


### Classes

Classes are reference types: They are always stored on the heap.
On assignment, a pointer is passed to the same location on the heap.

**Example**
```
class Foo
	{
	private int bar

	internal int baz()
		{
		return bar + 1
		}
	}
```

#### Inheritance

**Example**
```
class Foo : Bar
	{
	private int baz

	public int hoo(int har)
		{
		return baz - har
		}
	}
```

### Structs

Structs are value types: They can be stored either on the heap or the stack.
On assignment, all members are copied.
They are similar to classes. However, they cannot be inherited nor can they
have a member of their own type or of a struct that has this struct as a member.


## Special symbols

### Arithemic
- `+`: Addition
- `-`: Substraction
- `*`: Multiplication
- `/`: Division
- `%`: Remainder
- `**`: Exponentiate
- `%%`: Modulo

### Bitwisze
- `<<`: Shift left
- `>>`: Shift right
- `|`: Or
- `&`: And
- `^`: Xor

### Bool
- `==`: Equal
- `!=`: Not equal
- `>`: Greater than
- `<`: Less than
- `>=`: Greator or equal to
- `<=`: Less or equal to
- `||`: Or
- `&&`: And

### Other
- `{ ... }`: defines a scope.
- `[ ... ]`: Used as either an indexer or to create an array.
- `( ... )`: Used to call a function or group an expression.
- `\n` and `;`: Used to end a statement.
- `"`: Used to define a string.
- `'`: Used to define a character.


## Access modifiers
- `private`: Only visible within a class or struct.
- `protected`: Only visible within a class or struct and its children
- `internal`: Only visible within an assembly
- `public`: Visible outside an assembly.
