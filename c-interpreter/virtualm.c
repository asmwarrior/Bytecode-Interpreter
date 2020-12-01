#include <stdarg.h>	// for variadic functions
#include <stdio.h>


#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "virtualm.h"

// initialize virtual machine here
VM vm;


// forward declartion of run
static InterpretResult run();

static void resetStack()
{
	// point stackStop to the begininng of the empty array
	vm.stackTop = vm.stack;		// stack array(vm.stack) is already indirectly declared, hence no need to allocate memory for it
}

// IMPORTANT
// variadic function ( ... ), takes a varying number of arguments
static void runtimeError(const char* format, ...)
{
	va_list args;	// list from the varying parameter
	va_start(args, format);
	vprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);	// fputs; write a string to the stream but not including the null character

	// tell which line the error occurred
	size_t instruction = vm.ip - vm.chunk->code - 1;	
	int line = vm.chunk->lines[instruction];
	fprintf("Error in script at [Line %d]\n", line);

	resetStack();
}


void initVM()
{
	resetStack();
}

void freeVM()
{

}

/* stack operations */
void push(Value value)
{
	*vm.stackTop = value;		// * in front of the pointer means the rvalue itself, assign value(parameter) to it
	vm.stackTop++;
}

Value pop()
{
	vm.stackTop--;		// first move the stack BACK to get the last element(stackTop points to ONE beyond the last element)
	return  *vm.stackTop;
}
/* end of stack operations */

// PEEK from the STACK, AFTER the compiler passes it through
// return a value from top of the stack but does not pop it, distance being how far down
// this is a C kind of accessing arrays/pointers
static Value peek(int distance)
{
	return vm.stackTop[-1 - distance];
}


static bool isFalsey(Value value)
{
	// return true if value is the null type or if it is a false bool type
	//printf("compared");
	//return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
	bool test = IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));

	return test;
}

/* starting point of the compiler */
InterpretResult interpret(const char* source)
{
	Chunk chunk;			// declare chunk/bytecode for the compiler
	initChunk(&chunk);		// initialize the chunk

	// pass chunk to compiler and fill it with bytecode
	if (!compile(source, &chunk))		// if compilation fails
	{
		freeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;			// set vm chunk
	vm.ip = vm.chunk->code;		// assign pointer to the start of the chunk

	InterpretResult result = run();

	freeChunk(&chunk);
	return result;
}


// run the chunk
// most IMPORTANT part of the interpreter
static InterpretResult run()		// static means the scope of the function is only to this file
{

/* info on the macros below
Below macros are FUNCTIONSt that take ZERO arguments, and what is inside () is their return value
READ_BYTE:	
	macro to ACCESS the BYTE(uin8_t) from the POINTER(ip), and increment it
	reads byte currently pointed at ip, then advances the instruction pointer
READ_CONSTANT:
	return constants.values element, from READ_BYTE(), which points exactly to the NEXT index
*/

#define READ_BYTE() (*vm.ip++)		
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])	

// MACRO for binary operations
// take two last constants, and push ONE final value doing the operations on both of them
// this macro needs to expand to a series of statements, read a-virtual-machine for more info, this is a macro trick or a SCOPE BLOCK
// pass in an OPERAOTR as a MACRO
// valueType is a Value struct
// first check that both operands are numbers
#define BINARY_OP(valueType, op)	\
	do {	\
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))	\
		{	\
			runtimeError("Operands must be numbers.");	\
			return INTERPRET_RUNTIME_ERROR;		\
		}	\
		double b = AS_NUMBER(pop());	\
		double a = AS_NUMBER(pop());	\
		push(valueType(a op b));	\
	} while(false)	\

	for (;;)
	{
	// disassembleInstruction needs an byte offset, do pointer math to convert ip back to relative offset
	// from the beginning of the chunk (subtract current ip from the starting ip)
	// IMPORTANT -> only for debugging the VM
#ifdef DEBUG_TRACE_EXECUTION
		// for stack tracing
		printf("		");
		/* note on C POINTERSE
		-> pointing to the array itself means pointing to the start of the array, or the first element of the array
		-> ++/-- means moving through the array (by 1 or - 1)
		-> you can use operands like < > to tell compare how deep are you in the array
		*/

		// prints every existing value in the stack
		for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
		{
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t instruction;
		switch (instruction = READ_BYTE())			// get result of the byte read, every set of instruction starts with an opcode
		{
			case OP_CONSTANT: 
			{
				// function is smart; chunk advances by 1 on first read, then in the READ_CONSTANT() macro it reads again which advances by 1 and returns the INDEX
				Value constant = READ_CONSTANT();		// READ the next line, which is the INDEX of the constant in the constants array
				push(constant);		// push to stack
				break;			// break from the switch
			}
			// unary opcode
			case OP_NEGATE: 
				if (!IS_NUMBER(peek(0)))		// if next value is not a number
				{
					//printf("\nnot a number\n"); it actually works
					runtimeError("Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}
				
				push(NUMBER_VAL(-AS_NUMBER(pop()))); 
				break;  // negates the last element of the stack
			
			// literals
			case OP_NULL: push(NULL_VAL); break;
			case OP_TRUE: push(BOOL_VAL(true)); break;
			case OP_FALSE: push(BOOL_VAL(false)); break;

			// binary opcode
			case OP_ADD: BINARY_OP(NUMBER_VAL, +); break;			// initialize new Value struct (NUMBER_VAL) here
			case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
			case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;

			case OP_NOT:
				push(BOOL_VAL(isFalsey(pop())));		// again, pops most recent one from the stack, does the operation on it, and pushes it back
				break;

			case OP_EQUAL:		// implemenation comparison done here
			{
				Value b = pop();
				Value a = pop();
				push(BOOL_VAL(valuesEqual(a, b)));
				break;
			}
			case OP_GREATER: BINARY_OP(BOOL_VAL, > ); break;
			case OP_LESS: BINARY_OP(BOOL_VAL, < ); break;

			case OP_RETURN:				
			{
				// ACTUAL PRINTING IS DONE HERE
				printValue(pop());		// pop the stack and print the value, getting it from value.c
				printf("\n");
				return INTERPRET_OK;
			}
		}
	}


#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}