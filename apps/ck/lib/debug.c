/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    debug.c

Abstract:

    This module implements debug support in Chalk.

Author:

    Evan Green 22-Jun-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdarg.h>
#include <stdio.h>

#include "chalkp.h"
#include <minoca/lib/status.h>
#include <minoca/lib/yy.h>
#include "compiler.h"
#include "lang.h"
#include "compsup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpDebugPrint(
    PCK_VM Vm,
    PSTR Message,
    ...
    );

INTN
CkpDumpInstruction (
    PCK_VM Vm,
    PCK_FUNCTION Function,
    UINTN Offset,
    PLONG LastLine
    );

VOID
CkpDumpValue (
    PCK_VM Vm,
    CK_VALUE Value
    );

VOID
CkpDumpObject (
    PCK_VM Vm,
    PCK_OBJECT Object
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR CkOpcodeNames[CkOpcodeCount] = {
    "Nop",
    "Constant",
    "Null",
    "Literal0",
    "Literal1",
    "Literal2",
    "Literal3",
    "Literal4",
    "Literal5",
    "Literal6",
    "Literal7",
    "Literal8",
    "LoadLocal0",
    "LoadLocal1",
    "LoadLocal2",
    "LoadLocal3",
    "LoadLocal4",
    "LoadLocal5",
    "LoadLocal6",
    "LoadLocal7",
    "LoadLocal8",
    "LoadLocal",
    "StoreLocal",
    "LoadUpvalue",
    "StoreUpvalue",
    "LoadModuleVariable",
    "StoreModuleVariable",
    "LoadFieldThis",
    "StoreFieldThis",
    "LoadField",
    "StoreField",
    "Pop",
    "Call0",
    "Call1",
    "Call2",
    "Call3",
    "Call4",
    "Call5",
    "Call6",
    "Call7",
    "Call8",
    "Call",
    "IndirectCall",
    "SuperCall0",
    "SuperCall1",
    "SuperCall2",
    "SuperCall3",
    "SuperCall4",
    "SuperCall5",
    "SuperCall6",
    "SuperCall7",
    "SuperCall8",
    "SuperCall",
    "Jump",
    "Loop",
    "JumpIf",
    "And",
    "Or",
    "CloseUpvalue",
    "Return",
    "Closure",
    "Construct",
    "ForeignConstruct",
    "Class",
    "ForeignClass",
    "Method",
    "StaticMethod",
    "End"
};

PSTR CkObjectTypeNames[CkObjectTypeCount] = {
    "Invalid",
    "Class",
    "Closure",
    "Fiber",
    "Function",
    "Foreign",
    "Instance",
    "List",
    "Dict",
    "Module",
    "Range",
    "String",
    "Upvalue"
};

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpDumpCode (
    PCK_VM Vm,
    PCK_FUNCTION Function
    )

/*++

Routine Description:

    This routine prints the bytecode assembly for the given function.

Arguments:

    Vm - Supplies a pointer to the VM.

    Function - Supplies a pointer to the function containing the bytecode.

Return Value:

    None.

--*/

{

    LONG LastLine;
    PSTR Name;
    UINTN Offset;
    INTN Size;

    Name = Function->Module->Name->Value;
    if (Name == NULL) {
        Name = "<core>";
    }

    CkpDebugPrint(Vm, "%s: %s\n", Name, Function->Debug.Name);
    Offset = 0;
    LastLine = -1;
    while (TRUE) {
        Size = CkpDumpInstruction(Vm, Function, Offset, &LastLine);
        if (Size == -1) {
            break;
        }

        Offset += Size;
    }

    CkpDebugPrint(Vm, "\n");
    return;
}

INT
CkpGetLineForOffset (
    PCK_FUNCTION Function,
    UINTN CodeOffset
    )

/*++

Routine Description:

    This routine determines what line the given bytecode offset is on.

Arguments:

    Function - Supplies a pointer to the function containing the bytecode.

    CodeOffset - Supplies the offset whose line number is desired.

Return Value:

    Returns the line number the offset in question.

    -1 if no line number information could be found.

--*/

{

    PUCHAR End;
    LONG Line;
    PUCHAR LineProgram;
    UINTN Offset;
    CK_LINE_OP Op;

    LineProgram = Function->Debug.LineProgram.Data;
    End = LineProgram + Function->Debug.LineProgram.Count;
    Offset = 0;
    Line = Function->Debug.FirstLine;
    while (LineProgram < End) {
        Op = *LineProgram;
        LineProgram += 1;
        switch (Op) {
        case CkLineOpNop:
            break;

        case CkLineOpSetLine:
            CkCopy(&Line, LineProgram, sizeof(ULONG));
            LineProgram += sizeof(ULONG);
            break;

        case CkLineOpSetOffset:
            CkCopy(&Offset, LineProgram, sizeof(ULONG));
            LineProgram += sizeof(ULONG);
            break;

        case CkLineOpAdvanceLine:
            Line += CkpUtf8Decode(LineProgram, End - LineProgram);
            LineProgram += CkpUtf8DecodeSize(*LineProgram);
            break;

        case CkLineOpAdvanceOffset:
            Offset += CkpUtf8Decode(LineProgram, End - LineProgram);
            LineProgram += CkpUtf8DecodeSize(*LineProgram);
            break;

        case CkLineOpSpecial:
        default:
            Line += CK_LINE_ADVANCE(Op);
            Offset += CK_OFFSET_ADVANCE(Op);
            break;
        }

        if (Offset >= CodeOffset) {
            return Line;
        }
    }

    CK_ASSERT(FALSE);

    return -1;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpDebugPrint (
    PCK_VM Vm,
    PSTR Message,
    ...
    )

/*++

Routine Description:

    This routine prints something to the output for the debug code.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Message - Supplies the printf-style format message to print.

    ... - Supplies the remainder of the arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;
    CHAR Buffer[CK_MAX_ERROR_MESSAGE];

    va_start(ArgumentList, Message);
    vsnprintf(Buffer, sizeof(Buffer), Message, ArgumentList);
    va_end(ArgumentList);
    Buffer[sizeof(Buffer) - 1] = '\0';
    if (Vm->Configuration.Write != NULL) {
        Vm->Configuration.Write(Vm, Buffer);
    }

    return;
}

INTN
CkpDumpInstruction (
    PCK_VM Vm,
    PCK_FUNCTION Function,
    UINTN Offset,
    PLONG LastLine
    )

/*++

Routine Description:

    This routine prints the bytecode for a single instruction.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function.

    Offset - Supplies the offset into the function code to print from.

    LastLine - Supplies an optional pointer where the last line number printed
        is given on input. On output, returns the line number of this
        instruction.

Return Value:

    Returns the length of this instruction.

    -1 if there are no more instructions.

--*/

{

    PUCHAR ByteCode;
    CK_SYMBOL_INDEX Capture;
    CK_SYMBOL_INDEX Constant;
    BOOL IsLocal;
    INTN Jump;
    LONG Line;
    PCK_FUNCTION LoadedFunction;
    PSTR LocalType;
    CK_OPCODE Op;
    UINTN Start;
    CK_SYMBOL_INDEX Symbol;

    Start = Offset;
    ByteCode = Function->Code.Data;
    Op = ByteCode[Offset];
    Line = CkpGetLineForOffset(Function, Offset);
    CkpDebugPrint(Vm, "%4x ", Offset);
    if ((LastLine == NULL) || (*LastLine != Line)) {
        CkpDebugPrint(Vm, "%4d: ", Line);
        if (LastLine != NULL) {
            *LastLine = Line;
        }

    } else {
        CkpDebugPrint(Vm, "      ");
    }

    if (Op >= CkOpcodeCount) {
        CkpDebugPrint(Vm, "Unknown %d", Op);

    } else {
        CkpDebugPrint(Vm, "%s ", CkOpcodeNames[Op]);
    }

    Offset += 1;
    switch (Op) {
    case CkOpConstant:
        Constant = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Constant < Function->Constants.Count);

        CkpDumpValue(Vm, Function->Constants.Data[Constant]);
        break;

    case CkOpLoadModuleVariable:
    case CkOpStoreModuleVariable:
        Symbol = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Symbol < Function->Module->VariableNames.Count);

        CkpDebugPrint(Vm,
                      "%s",
                      Function->Module->VariableNames.Data[Symbol].Data);

        break;

    case CkOpCall:
    case CkOpSuperCall:
        Symbol = CK_READ8(ByteCode + Offset);
        Offset += 1;
        CkpDebugPrint(Vm, "%d ", Symbol);

    //
    // Fall through
    //

    case CkOpCall0:
    case CkOpCall1:
    case CkOpCall2:
    case CkOpCall3:
    case CkOpCall4:
    case CkOpCall5:
    case CkOpCall6:
    case CkOpCall7:
    case CkOpCall8:
    case CkOpSuperCall0:
    case CkOpSuperCall1:
    case CkOpSuperCall2:
    case CkOpSuperCall3:
    case CkOpSuperCall4:
    case CkOpSuperCall5:
    case CkOpSuperCall6:
    case CkOpSuperCall7:
    case CkOpSuperCall8:
    case CkOpMethod:
    case CkOpStaticMethod:
        Symbol = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Symbol < Vm->MethodNames.Count);

        CkpDebugPrint(Vm, "%s", Vm->MethodNames.Data[Symbol].Data);
        break;

    case CkOpIndirectCall:
    case CkOpLoadLocal:
    case CkOpStoreLocal:
    case CkOpLoadUpvalue:
    case CkOpStoreUpvalue:
    case CkOpLoadFieldThis:
    case CkOpStoreFieldThis:
    case CkOpLoadField:
    case CkOpStoreField:
        Constant = CK_READ8(ByteCode + Offset);
        Offset += 1;
        CkpDebugPrint(Vm, "%d", Constant);
        break;

    case CkOpJump:
    case CkOpJumpIf:
    case CkOpAnd:
    case CkOpOr:
        Jump = CK_READ16(ByteCode + Offset);
        Offset += 2;
        CkpDebugPrint(Vm, "%x", Offset + Jump);
        break;

    case CkOpLoop:
        Jump = CK_READ16(ByteCode + Offset);
        Offset += 2;
        CkpDebugPrint(Vm, "%x", Offset - Jump);
        break;

    case CkOpClosure:
        Constant = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Constant < Function->Constants.Count);

        LoadedFunction = CK_AS_FUNCTION(Function->Constants.Data[Constant]);
        CkpDumpValue(Vm, Function->Constants.Data[Constant]);
        CkpDebugPrint(Vm, " ");
        for (Capture = 0;
             Capture < LoadedFunction->UpvalueCount;
             Capture += 1) {

            IsLocal = CK_READ8(ByteCode + Offset);
            Offset += 1;
            Symbol = CK_READ8(ByteCode + Offset);
            Offset += 1;
            if (Capture > 0) {
                CkpDebugPrint(Vm, ", ");
            }

            LocalType = "upvalue";
            if (IsLocal != FALSE) {
                LocalType = "local";
            }

            CkpDebugPrint(Vm, "%s %d", LocalType, Symbol);
        }

        break;

    case CkOpClass:
        Constant = CK_READ8(ByteCode + Offset);
        Offset += 1;
        CkpDebugPrint(Vm, "%d fields", Constant);
        break;

    default:
        break;
    }

    CkpDebugPrint(Vm, "\n");
    if (Op == CkOpEnd) {
        return -1;
    }

    return Offset - Start;
}

VOID
CkpDumpValue (
    PCK_VM Vm,
    CK_VALUE Value
    )

/*++

Routine Description:

    This routine prints the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Value - Supplies the value to print.

Return Value:

    None.

--*/

{

    switch (Value.Type) {
    case CkValueNull:
        CkpDebugPrint(Vm, "null");
        break;

    case CkValueInteger:
        CkpDebugPrint(Vm, "%lld", CK_AS_INTEGER(Value));
        break;

    case CkValueObject:
        CkpDumpObject(Vm, CK_AS_OBJECT(Value));
        break;

    default:

        CK_ASSERT(FALSE);

        CkpDebugPrint(Vm, "<invalid object>");
        break;
    }

    return;
}

VOID
CkpDumpObject (
    PCK_VM Vm,
    PCK_OBJECT Object
    )

/*++

Routine Description:

    This routine prints the given object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to print.

Return Value:

    None.

--*/

{

    switch (Object->Type) {
    case CkObjectString:
        CkpDebugPrint(Vm, "\"%s\"", ((PCK_STRING_OBJECT)Object)->Value);
        break;

    default:
        if (Object->Type < CkObjectTypeCount) {
            CkpDebugPrint(Vm,
                          "<%s %p>",
                          CkObjectTypeNames[Object->Type],
                          Object);

        } else {
            CkpDebugPrint(Vm, "<unknown %d %p>", Object->Type, Object);
        }

        break;
    }

    return;
}