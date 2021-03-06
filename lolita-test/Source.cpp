#include "testheader.h"
#include "lolita-include.h"
#include "text\format.h"
#include <memory>
#include <fstream>
#include <iostream>
#include <chrono>

using namespace std;
using namespace eds::text;
using namespace eds::loli;

std::string LoadConfigText()
{
    std::fstream file{"d:\\lang.loli.txt"};
    return std::string(std::istreambuf_iterator<char>{file}, {});
}

auto kConfig = LoadConfigText();
auto kSample = string{
    "func add(x: int, y: int) -> int { return x+y; }\n"
    "func mul(x: int, y: int) -> int { return x*y; }\n"
    "func main() -> unit { if(true) while(true) if(true) {} else {} else val x:int=41; }\n"};

//*
string ToString_Token(const BasicAstToken& tok)
{
    return kSample.substr(tok.Offset(), tok.Length());
}
string ToString_Type(Type* type)
{
    const auto& t = reinterpret_cast<NamedType*>(type);
    return ToString_Token(t->name());
}
string ToString_Expr(Expression* expr)
{
    return kSample.substr(expr->Offset(), expr->Length());
}

void PrintIdent(int ident)
{
    for (int i = 0; i < ident; ++i)
        putchar(' ');
}

void PrintStatement(Statement* s, int ident)
{
    struct Visitor : Statement::Visitor
    {
        int ident;

        void Visit(VariableDeclStmt& stmt) override
        {
            PrintIdent(ident);

            auto mut = stmt.mut() == VariableMutability::Val ? "mutable" : "immutable";
            PrintFormatted("Variable Decl ({}) {} of {}\n", mut, ToString_Token(stmt.name()), ToString_Type(stmt.type()));
        }
        void Visit(JumpStmt& stmt) override
        {
            PrintIdent(ident);

            if (stmt.command() == JumpCommand::Break)
            {
                PrintFormatted("Break\n");
            }
            else // JumpCommand::Continue
            {
                PrintFormatted("Continue\n");
            }
        }
        void Visit(ReturnStmt& stmt) override
        {
            PrintIdent(ident);
            PrintFormatted("Return {}\n", ToString_Expr(stmt.expr()));
        }
        void Visit(CompoundStmt& stmt) override
        {
            PrintIdent(ident);
            if (stmt.children()->Empty())
                PrintFormatted("Empty compound\n");
            else
                PrintFormatted("Compound\n");

            for (auto child : stmt.children()->Value())
            {
                PrintStatement(child, ident + 4);
            }
        }
        void Visit(WhileStmt& stmt) override
        {
            PrintIdent(ident);
            PrintFormatted("While {}\n", ToString_Expr(stmt.pred()));

            PrintStatement(stmt.body(), ident + 4);
        }
        void Visit(ChoiceStmt& stmt) override
        {
            PrintIdent(ident);
            PrintFormatted("Choice {}\n", ToString_Expr(stmt.pred()));

            PrintIdent(ident);
            PrintFormatted("Positive:\n");
            PrintStatement(stmt.positive(), ident + 4);

            if (const auto& negative = stmt.negative(); negative.HasValue())
            {
                PrintIdent(ident);
                PrintFormatted("Negative:\n");
                PrintStatement(negative.Value(), ident + 4);
            }
        }
    };

    auto v  = Visitor{};
    v.ident = ident;
    s->Accept(v);
}

void PrintFunctionDecl(FuncDecl* f)
{
    PrintFormatted("Function {}@(offset:{}, length:{})\n", ToString_Token(f->name()), f->Offset(), f->Length());

    PrintFormatted("Parameters:\n");
    for (auto param : f->params()->Value())
        PrintFormatted("    {} of {}\n", ToString_Token(param->name()), ToString_Type(param->type()));

    PrintFormatted("Returns:\n");
    PrintFormatted("    {}\n", ToString_Type(f->ret()));

    PrintFormatted("Body: [\n");
    for (auto stmt : f->body()->Value())
        PrintStatement(stmt, 4);

    PrintFormatted("]\n");
}
void PrintTranslationUnit(const TranslationUnit& u)
{
    for (auto f : u.functions()->Value())
        PrintFunctionDecl(f);
}
//*/

int main()
{
    //cout << BootstrapParser(kConfig);
    auto parser = CreateParser();
    auto result = parser->Parse(kSample);
    PrintTranslationUnit(*result.result);

    system("pause");
}