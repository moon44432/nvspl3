
// SEL Project
// parser.cpp

#include "ast.h"
#include "lexer.h"
#include <iostream>
#include <cmath>

int CurTok;
std::map<std::string, int> BinopPrecedence;
std::string OpChr = "<>+-*/%!&|=";

void binopPrecInit()
{
    BinopPrecedence["**"] = 18 - 4; // highest
    BinopPrecedence["*"] = 18 - 5;
    BinopPrecedence["/"] = 18 - 5;
    BinopPrecedence["%"] = 18 - 5;
    BinopPrecedence["+"] = 18 - 6;
    BinopPrecedence["-"] = 18 - 6;
    BinopPrecedence["<"] = 18 - 8;
    BinopPrecedence[">"] = 18 - 8;
    BinopPrecedence["<="] = 18 - 8;
    BinopPrecedence[">="] = 18 - 8;
    BinopPrecedence["=="] = 18 - 9;
    BinopPrecedence["!="] = 18 - 9;
    BinopPrecedence["&&"] = 18 - 13;
    BinopPrecedence["||"] = 18 - 14;
    BinopPrecedence["="] = 18 - 15; // lowest
}

int getNextToken()
{
    return CurTok = gettok();
}

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int GetTokPrecedence(std::string Op)
{
    // Make sure it's a declared binop.
    int TokPrec = BinopPrecedence[Op];
    if (TokPrec <= 0)
        return -1;
    return TokPrec;
}

/// LogError* - These are little helper functions for error handling.
std::shared_ptr<ExprAST> LogError(const char* Str)
{
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::shared_ptr<PrototypeAST> LogErrorP(const char* Str)
{
    LogError(Str);
    return nullptr;
}

/// numberexpr ::= number
std::shared_ptr<ExprAST> ParseNumberExpr()
{
    auto Result = std::make_shared<NumberExprAST>(NumVal);
    getNextToken(); // consume the number
    return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
std::shared_ptr<ExprAST> ParseParenExpr()
{
    getNextToken(); // eat (.
    auto V = ParseExpression();
    if (!V)
        return nullptr;

    if (CurTok != ')')
        return LogError("expected ')'");
    getNextToken(); // eat ).
    return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier ('[' expression ']')+
///   ::= identifier '(' expression* ')'
std::shared_ptr<ExprAST> ParseIdentifierExpr()
{
    std::string IdName = IdentifierStr;

    getNextToken(); // eat identifier.

    if (CurTok != '(') // simple variable or array element ref.
    {
        if (CurTok != '[') return std::make_shared<VariableExprAST>(IdName);
        getNextToken();

        std::vector<std::shared_ptr<ExprAST>> Indices;
        if (CurTok != ']')
        {
            while (true)
            {
                if (auto Idx = ParseExpression())
                    Indices.push_back(std::move(Idx));
                else return nullptr;

                if (CurTok == ']')
                {
                    getNextToken();

                    if (CurTok != '[') break;
                    else getNextToken();
                }
            }
        }
        else return LogError("array index missing");

        return std::make_shared<VariableExprAST>(IdName, Indices);
    }

    // Call.
    getNextToken(); // eat (
    std::vector<std::shared_ptr<ExprAST>> Args;
    if (CurTok != ')')
    {
        while (true)
        {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else return nullptr;

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }

    // Eat the ')'.
    getNextToken();

    return std::make_shared<CallExprAST>(IdName, std::move(Args));
}

/// arrdeclexpr ::= 'arr' identifier ('[' number ']')+
std::shared_ptr<ExprAST> ParseArrDeclExpr()
{
    getNextToken(); // eat the arr.

    std::string IdName = IdentifierStr;
    getNextToken();

    if (CurTok != '[') return LogError("expected '[' after array name");

    getNextToken();

    std::vector<int> Indices;

    if (CurTok != ']')
    {
        while (true)
        {
            if (CurTok == tok_number && fmod(NumVal, 1.0) == 0)
            {
                if ((unsigned)NumVal >= 1) Indices.push_back((unsigned)NumVal);
                else return LogError("length of each dimension must be 1 or higher");
            }
            else return LogError("length of each dimension must be an integer");
            getNextToken();

            if (CurTok == ']')
            {
                if (LastChar != '[')
                    break;

                getNextToken();
                getNextToken();
            }
        }
    }
    else return LogError("array dimension missing");

    getNextToken();
    return std::make_shared<ArrDeclExprAST>(IdName, Indices);
}

/// ifexpr ::= 'if' expression 'then' blockexpr 'else' blockexpr
std::shared_ptr<ExprAST> ParseIfExpr()
{
    getNextToken(); // eat the if.

    // condition.
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != tok_then)
        return LogError("expected then");
    getNextToken(); // eat the then

    auto Then = ParseBlockExpression();
    if (!Then)
        return nullptr;

    if (CurTok == tok_else)
    {
        getNextToken();

        auto Else = ParseBlockExpression();
        if (!Else)
            return nullptr;

        return std::make_shared<IfExprAST>(std::move(Cond), std::move(Then),
            std::move(Else));
    }
    else return std::make_shared<IfExprAST>(std::move(Cond), std::move(Then));
}

/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? blockexpr
std::shared_ptr<ExprAST> ParseForExpr()
{
    getNextToken(); // eat the for.

    if (CurTok != tok_identifier)
        return LogError("expected identifier");

    std::string IdName = IdentifierStr;
    getNextToken(); // eat identifier.

    if (CurTok != '=')
        return LogError("expected '=' after for");
    getNextToken(); // eat '='.

    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (CurTok != ',')
        return LogError("expected ',' after for start value");
    getNextToken();

    auto End = ParseExpression();
    if (!End)
        return nullptr;

    // The step value is optional.
    std::shared_ptr<ExprAST> Step;
    if (CurTok == ',') {
        getNextToken();
        Step = ParseExpression();
        if (!Step)
            return nullptr;
    }

    auto Body = ParseBlockExpression();
    if (!Body)
        return nullptr;

    return std::make_shared<ForExprAST>(IdName, std::move(Start), std::move(End),
        std::move(Step), std::move(Body));
}

/// whileexpr ::= 'while' expr blockexpr
std::shared_ptr<ExprAST> ParseWhileExpr()
{
    getNextToken(); // eat the while.

    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    auto Body = ParseBlockExpression();
    if (!Body)
        return nullptr;

    return std::make_shared<WhileExprAST>(std::move(Cond), std::move(Body));
}

/// reptexpr ::= 'rept' '(' expr ')' blockexpr
std::shared_ptr<ExprAST> ParseRepeatExpr()
{
    getNextToken(); // eat the rept.

    if (CurTok != '(')
        return LogError("expected '(' after rept");
    getNextToken();

    auto IterNum = ParseExpression();
    if (!IterNum)
        return nullptr;

    if (CurTok != ')')
        return LogError("expected ')'");
    getNextToken(); // eat ')'.

    auto Body = ParseBlockExpression();
    if (!Body)
        return nullptr;

    return std::make_shared<RepeatExprAST>(std::move(IterNum), std::move(Body));
}


/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
///   ::= ifexpr
///   ::= forexpr
///   ::= whileexpr
///   ::= reptexpr
///   ::= arrdeclexpr
std::shared_ptr<ExprAST> ParsePrimary()
{
    switch (CurTok) {
    default:
        return LogError("unknown token when expecting an expression");
    case tok_identifier:
        return ParseIdentifierExpr();
    case tok_number:
        return ParseNumberExpr();
    case tok_for:
        return ParseForExpr();
    case tok_while:
        return ParseWhileExpr();
    case tok_if:
        return ParseIfExpr();
    case tok_repeat:
        return ParseRepeatExpr();
    case tok_arr:
        return ParseArrDeclExpr();
    case '(': // must be the last one (for, var, if, etc also use '(')
        return ParseParenExpr();
    }
}

/// unary
///   ::= primary
///   ::= unaryop unary
std::shared_ptr<ExprAST> ParseUnary()
{
    // If the current token is not an operator, it must be a primary expr.
    if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
        return ParsePrimary();

    // If this is a unary operator, read it.
    int Opc;
    if (OpChr.find(CurTok) != std::string::npos)
    {
        Opc = CurTok;
        getNextToken();
    }
    else return LogError(((std::string)"unknown token '" + (char)CurTok + (std::string)"'").c_str());

    if (auto Operand = ParseUnary())
        return std::make_shared<UnaryExprAST>(Opc, std::move(Operand));
    return nullptr;
}

/// binoprhs
///   ::= (binop unary)*
std::shared_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::shared_ptr<ExprAST> LHS)
{
    // If this is a binop, find its precedence.
    while (true)
    {
        bool DoubleCh = false;
        std::string BinOp;
        BinOp += (char)CurTok;

        if (OpChr.find(CurTok) != std::string::npos && 
            OpChr.find(LastChar) != std::string::npos)
        {
            BinOp += (char)LastChar;
            DoubleCh = true;
        }
        int TokPrec = GetTokPrecedence(BinOp);

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (TokPrec < ExprPrec)
            return LHS;

        getNextToken();
        if (DoubleCh) getNextToken();

        // Parse the unary expression after the binary operator.
        auto RHS = ParseUnary();
        if (!RHS)
            return nullptr;

        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        std::string NextOp;
        NextOp += (char)CurTok;

        if (OpChr.find(CurTok) != std::string::npos &&
            OpChr.find(LastChar) != std::string::npos)
        {
            NextOp += (char)LastChar;
        }

        int NextPrec = GetTokPrecedence(NextOp);
        if (TokPrec < NextPrec)
        {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS) return nullptr;
        }

        // Merge LHS/RHS.
        LHS =
            std::make_shared<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

/// expression
///   ::= unary binoprhs
std::shared_ptr<ExprAST> ParseExpression()
{
    auto LHS = ParseUnary();
    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}

/// blockexpr
///   ::= expression
///   ::= '{' expression+ '}'
std::shared_ptr<ExprAST> ParseBlockExpression()
{
    if (CurTok != tok_openblock)
        return ParseExpression();
    getNextToken();

    std::vector<std::shared_ptr<ExprAST>> ExprSeq;

    while (true)
    {
        auto Expr = ParseBlockExpression();
        ExprSeq.push_back(std::move(Expr));
        if (CurTok == ';')
            getNextToken();
        if (CurTok == tok_closeblock)
        {
            getNextToken();
            break;
        }
    }
    auto Block = std::make_shared<BlockExprAST>(std::move(ExprSeq));
    return std::move(Block);
}

/// prototype
///   ::= id '(' id* ')'
///   ::= binary LETTER(LETTER)? number? (id, id)
///   ::= unary LETTER (id)
std::shared_ptr<PrototypeAST> ParsePrototype()
{
    std::string FnName;

    unsigned Kind = 0; // 0 = identifier, 1 = unary, 2 = binary.
    unsigned BinaryPrecedence = 18;

    switch (CurTok) {
    default:
        return LogErrorP("Expected function name in prototype");
    case tok_identifier:
        FnName = IdentifierStr;
        Kind = 0;
        getNextToken();
        break;
    case tok_unary:
        getNextToken();
        if (!isascii(CurTok))
            return LogErrorP("Expected unary operator");
        FnName = "unary";
        FnName += (char)CurTok;
        Kind = 1;
        getNextToken();
        break;
    case tok_binary:
        getNextToken();
        if (!isascii(CurTok))
            return LogErrorP("Expected binary operator");

        std::string OpName;
        OpName += (char)CurTok;
        getNextToken();
        if (OpChr.find(CurTok) != std::string::npos)
        {
            OpName += (char)CurTok;
            getNextToken();
        }
        Kind = 2;

        // Read the precedence if present.
        if (CurTok == tok_number) {
            if (NumVal < 1 || NumVal > 18)
                return LogErrorP("Invalid precedence: must be 1~18");
            BinaryPrecedence = (unsigned)NumVal;
            getNextToken();
        }

        // install binary operator.
        BinopPrecedence[OpName] = BinaryPrecedence;

        FnName = "binary" + OpName;
        break;
    }

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    while (true)
    {
        if (getNextToken() == tok_identifier)
            ArgNames.push_back(IdentifierStr);

        getNextToken();
        if (CurTok == ')') break;
        if (CurTok != ',')
            return LogErrorP("Expected ',' or ')'");
    }
    // success.
    getNextToken(); // eat ')'

    // Verify right number of names for operator.
    if (Kind && ArgNames.size() != Kind)
        return LogErrorP("Invalid number of operands for operator");

    return std::make_shared<PrototypeAST>(FnName, ArgNames, Kind != 0,
        BinaryPrecedence);
}

/// definition ::= 'func' prototype expression
std::shared_ptr<FunctionAST> ParseDefinition()
{
    getNextToken(); // eat func.
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;

    if (auto E = ParseBlockExpression())
        return std::make_shared<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

/// toplevelexpr ::= expression
std::shared_ptr<FunctionAST> ParseTopLevelExpr()
{
    if (auto E = ParseBlockExpression()) {
        // Make an anonymous proto.
        auto Proto = std::make_shared<PrototypeAST>("__anon_expr",
            std::vector<std::string>());
        return std::make_shared<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}