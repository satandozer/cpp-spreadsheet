#include <limits>

#include "common.h"
#include "formula.h"
#include "test_runner_p.h"

inline std::ostream& operator<<(std::ostream& output, Position pos) {
    return output << "(" << pos.row << ", " << pos.col << ")";
}

inline Position operator"" _pos(const char* str, std::size_t) {
    return Position::FromString(str);
}

inline std::ostream& operator<<(std::ostream& output, Size size) {
    return output << "(" << size.rows << ", " << size.cols << ")";
}

inline std::ostream& operator<<(std::ostream& output, const CellInterface::Value& value) {
    std::visit(
        [&](const auto& x) {
            output << x;
        },
        value);
    return output;
}

namespace {

void TestPositionAndStringConversion() {
    auto testSingle = [](Position pos, std::string_view str) {
        ASSERT_EQUAL(pos.ToString(), str);
        ASSERT_EQUAL(Position::FromString(str), pos);
    };

    for (int i = 0; i < 25; ++i) {
        testSingle(Position{i, i}, char('A' + i) + std::to_string(i + 1));
    }

    testSingle(Position{0, 0}, "A1");
    testSingle(Position{0, 1}, "B1");
    testSingle(Position{0, 25}, "Z1");
    testSingle(Position{0, 26}, "AA1");
    testSingle(Position{0, 27}, "AB1");
    testSingle(Position{0, 51}, "AZ1");
    testSingle(Position{0, 52}, "BA1");
    testSingle(Position{0, 53}, "BB1");
    testSingle(Position{0, 77}, "BZ1");
    testSingle(Position{0, 78}, "CA1");
    testSingle(Position{0, 701}, "ZZ1");
    testSingle(Position{0, 702}, "AAA1");
    testSingle(Position{136, 2}, "C137");
    testSingle(Position{Position::MAX_ROWS - 1, Position::MAX_COLS - 1}, "XFD16384");
}

void TestPositionToStringInvalid() {
    ASSERT_EQUAL((Position{-1, -1}).ToString(), "");
    ASSERT_EQUAL((Position{-10, 0}).ToString(), "");
    ASSERT_EQUAL((Position{1, -3}).ToString(), "");
}

void TestStringToPositionInvalid() {
    ASSERT(!Position::FromString("").IsValid());
    ASSERT(!Position::FromString("A").IsValid());
    ASSERT(!Position::FromString("1").IsValid());
    ASSERT(!Position::FromString("e2").IsValid());
    ASSERT(!Position::FromString("A0").IsValid());
    ASSERT(!Position::FromString("A-1").IsValid());
    ASSERT(!Position::FromString("A+1").IsValid());
    ASSERT(!Position::FromString("R2D2").IsValid());
    ASSERT(!Position::FromString("C3PO").IsValid());
    ASSERT(!Position::FromString("XFD16385").IsValid());
    ASSERT(!Position::FromString("XFE16384").IsValid());
    ASSERT(!Position::FromString("A1234567890123456789").IsValid());
    ASSERT(!Position::FromString("ABCDEFGHIJKLMNOPQRS8").IsValid());
}

void TestEmpty() {
    auto sheet = CreateSheet();
    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{0, 0}));
}

void TestInvalidPosition() {
    auto sheet = CreateSheet();
    try {
        sheet->SetCell(Position{-1, 0}, "");
    } catch (const InvalidPositionException&) {
    }
    try {
        sheet->GetCell(Position{0, -2});
    } catch (const InvalidPositionException&) {
    }
    try {
        sheet->ClearCell(Position{Position::MAX_ROWS, 0});
    } catch (const InvalidPositionException&) {
    }
}

void TestSetCellPlainText() {
    auto sheet = CreateSheet();
    //std::cout << "1";

    auto checkCell = [&](Position pos, std::string text) {
        sheet->SetCell(pos, text);
        CellInterface* cell = sheet->GetCell(pos);
        ASSERT(cell != nullptr);
        ASSERT_EQUAL(cell->GetText(), text);
        ASSERT_EQUAL(std::get<std::string>(cell->GetValue()), text);
    };

    checkCell("A1"_pos, "Hello");
    //std::cout << "2";   
    checkCell("A1"_pos, "World");
    //std::cout << "3";
    checkCell("B2"_pos, "Purr");
    //std::cout << "4";
    checkCell("A3"_pos, "Meow");
    //std::cout << "5";

    const SheetInterface& constSheet = *sheet;
    //std::cout << "6";
    ASSERT_EQUAL(constSheet.GetCell("B2"_pos)->GetText(), "Purr");
    //std::cout << "7";

    sheet->SetCell("A3"_pos, "'=escaped");
    //std::cout << "8";
    CellInterface* cell = sheet->GetCell("A3"_pos);
    //std::cout << "9";
    ASSERT_EQUAL(cell->GetText(), "'=escaped");
    //std::cout << "10";
    ASSERT_EQUAL(std::get<std::string>(cell->GetValue()), "=escaped");
    //std::cout << "11" << std::endl;
}

void TestClearCell() {
    auto sheet = CreateSheet();

    sheet->SetCell("C2"_pos, "Me gusta");
    sheet->ClearCell("C2"_pos);
    ASSERT(sheet->GetCell("C2"_pos) == nullptr);

    sheet->ClearCell("A1"_pos);
    sheet->ClearCell("J10"_pos);
}

void TestFormulaArithmetic() {
    auto sheet = CreateSheet();
    auto evaluate = [&](std::string expr) {
        return std::get<double>(ParseFormula(std::move(expr))->Evaluate(*sheet));
    };

    ASSERT_EQUAL(evaluate("1"), 1);
    ASSERT_EQUAL(evaluate("42"), 42);
    ASSERT_EQUAL(evaluate("2 + 2"), 4);
    ASSERT_EQUAL(evaluate("2 + 2*2"), 6);
    ASSERT_EQUAL(evaluate("4/2 + 6/3"), 4);
    ASSERT_EQUAL(evaluate("(2+3)*4 + (3-4)*5"), 15);
    ASSERT_EQUAL(evaluate("(12+13) * (14+(13-24/(1+1))*55-46)"), 575);
}

void TestFormulaReferences() {
    auto sheet = CreateSheet();
    auto evaluate = [&](std::string expr) {
        return std::get<double>(ParseFormula(std::move(expr))->Evaluate(*sheet));
    };

    sheet->SetCell("A1"_pos, "1");
    ASSERT_EQUAL(evaluate("A1"), 1);
    sheet->SetCell("A2"_pos, "2");
    ASSERT_EQUAL(evaluate("A1+A2"), 3);

    // Тест на нули:
    sheet->SetCell("B3"_pos, "");
    ASSERT_EQUAL(evaluate("A1+B3"), 1);  // Ячейка с пустым текстом
    ASSERT_EQUAL(evaluate("A1+B1"), 1);  // Пустая ячейка
    ASSERT_EQUAL(evaluate("A1+E4"), 1);  // Ячейка за пределами таблицы
}

void TestFormulaExpressionFormatting() {
    auto reformat = [](std::string expr) {
        return ParseFormula(std::move(expr))->GetExpression();
    };

    ASSERT_EQUAL(reformat("  1  "), "1");
    ASSERT_EQUAL(reformat("  -1  "), "-1");
    ASSERT_EQUAL(reformat("2 + 2"), "2+2");
    ASSERT_EQUAL(reformat("(2*3)+4"), "2*3+4");
    ASSERT_EQUAL(reformat("(2*3)-4"), "2*3-4");
    ASSERT_EQUAL(reformat("( ( (  1) ) )"), "1");
}

void TestFormulaReferencedCells() {
    ASSERT(ParseFormula("1")->GetReferencedCells().empty());

    auto a1 = ParseFormula("A1");
    ASSERT_EQUAL(a1->GetReferencedCells(), (std::vector{"A1"_pos}));

    auto b2c3 = ParseFormula("B2+C3");
    ASSERT_EQUAL(b2c3->GetReferencedCells(), (std::vector{"B2"_pos, "C3"_pos}));

    auto tricky = ParseFormula("A1 + A2 + A1 + A3 + A1 + A2 + A1");
    ASSERT_EQUAL(tricky->GetExpression(), "A1+A2+A1+A3+A1+A2+A1");
    ASSERT_EQUAL(tricky->GetReferencedCells(), (std::vector{"A1"_pos, "A2"_pos, "A3"_pos}));
}

void TestErrorValue() {
    auto sheet = CreateSheet();
    sheet->SetCell("E2"_pos, "A1");
    sheet->SetCell("E4"_pos, "=E2");
    ASSERT_EQUAL(sheet->GetCell("E4"_pos)->GetValue(),
                    CellInterface::Value(FormulaError::Category::Value));

    sheet->SetCell("E2"_pos, "3D");
    ASSERT_EQUAL(sheet->GetCell("E4"_pos)->GetValue(),
                    CellInterface::Value(FormulaError::Category::Value));
}

void TestErrorArithmetic() {
    auto sheet = CreateSheet();

    constexpr double max = std::numeric_limits<double>::max();

    sheet->SetCell("A1"_pos, "=1/0");
    ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetValue(),
                    CellInterface::Value(FormulaError::Category::Arithmetic));

    sheet->SetCell("A1"_pos, "=1e+200/1e-200");
    ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetValue(),
                    CellInterface::Value(FormulaError::Category::Arithmetic));

    sheet->SetCell("A1"_pos, "=0/0");
    ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetValue(),
                    CellInterface::Value(FormulaError::Category::Arithmetic));

    {
        std::ostringstream formula;
        formula << '=' << max << '+' << max;
        sheet->SetCell("A1"_pos, formula.str());
        ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetValue(),
                        CellInterface::Value(FormulaError::Category::Arithmetic));
    }

    {
        std::ostringstream formula;
        formula << '=' << -max << '-' << max;
        sheet->SetCell("A1"_pos, formula.str());
        ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetValue(),
                        CellInterface::Value(FormulaError::Category::Arithmetic));
    }

    {
        std::ostringstream formula;
        formula << '=' << max << '*' << max;
        sheet->SetCell("A1"_pos, formula.str());
        ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetValue(),
                        CellInterface::Value(FormulaError::Category::Arithmetic));
    }
}

void TestEmptyCellTreatedAsZero() {
    auto sheet = CreateSheet();
    sheet->SetCell("A1"_pos, "=B2");
    ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetValue(), CellInterface::Value(0.0));
}

void TestFormulaInvalidPosition() {
    auto sheet = CreateSheet();
    auto try_formula = [&](const std::string& formula) {
        try {
            sheet->SetCell("A1"_pos, formula);
            ASSERT(false);
        } catch (const FormulaException&) {
            // we expect this one
        }
    };
    try_formula("=X0");
    //std::cout << "1";
    try_formula("=ABCD1");
    //std::cout << "2";
    try_formula("=A123456");
    //std::cout << "3";
    try_formula("=ABCDEFGHIJKLMNOPQRS1234567890");
    //std::cout << "4";
    try_formula("=XFD16385");
    //std::cout << "5";
    try_formula("=XFE16384");
    //std::cout << "6";
    try_formula("=R2D2");
    //std::cout << "7";
}

void TestPrint() {
    auto sheet = CreateSheet();
    sheet->SetCell("A2"_pos, "meow");
    sheet->SetCell("B2"_pos, "=35");

    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{2, 2}));

    std::ostringstream texts;
    sheet->PrintTexts(texts);
    ASSERT_EQUAL(texts.str(), "\t\nmeow\t=35\n");

    std::ostringstream values;
    sheet->PrintValues(values);
    ASSERT_EQUAL(values.str(), "\t\nmeow\t35\n");
}

void TestCellReferences() {
    auto sheet = CreateSheet();
    sheet->SetCell("A1"_pos, "1");
    sheet->SetCell("A2"_pos, "=A1");
    sheet->SetCell("B2"_pos, "=A1");

    ASSERT(sheet->GetCell("A1"_pos)->GetReferencedCells().empty());
    ASSERT_EQUAL(sheet->GetCell("A2"_pos)->GetReferencedCells(), std::vector{"A1"_pos});
    ASSERT_EQUAL(sheet->GetCell("B2"_pos)->GetReferencedCells(), std::vector{"A1"_pos});
    // Ссылка на пустую ячейку
    sheet->SetCell("B2"_pos, "=B1");
    ASSERT(sheet->GetCell("B1"_pos)->GetReferencedCells().empty());
    ASSERT_EQUAL(sheet->GetCell("B2"_pos)->GetReferencedCells(), std::vector{"B1"_pos});
    sheet->SetCell("A2"_pos, "");
    ASSERT(sheet->GetCell("A1"_pos)->GetReferencedCells().empty());
    ASSERT(sheet->GetCell("A2"_pos)->GetReferencedCells().empty());
    // Ссылка на ячейку за пределами таблицы
    sheet->SetCell("B1"_pos, "=C3");
    ASSERT_EQUAL(sheet->GetCell("B1"_pos)->GetReferencedCells(), std::vector{"C3"_pos});
}

void TestFormulaIncorrect() {
    auto isIncorrect = [](std::string expression) {
        try {
            ParseFormula(std::move(expression));
        } catch (const FormulaException&) {
            return true;
        }
        return false;
    };

    ASSERT(isIncorrect("A2B"));
    ASSERT(isIncorrect("3X"));
    ASSERT(isIncorrect("A0++"));
    ASSERT(isIncorrect("((1)"));
    ASSERT(isIncorrect("2+4-"));
}

void TestCellCircularReferences() {
    auto sheet = CreateSheet();
    sheet->SetCell("E2"_pos, "=E4");
    sheet->SetCell("E4"_pos, "=X9");
    sheet->SetCell("X9"_pos, "=M6");
    sheet->SetCell("M6"_pos, "Ready");

    bool caught = false;
    try {
        sheet->SetCell("M6"_pos, "=E2");
    } catch (const CircularDependencyException&) {
        caught = true;
    }
    ASSERT(caught);
    ASSERT_EQUAL(sheet->GetCell("M6"_pos)->GetText(), "Ready");
}

void TestSetGetCellCellRef() {
    auto sheet = CreateSheet();
    auto checkCell = [&sheet](Position pos, std::string text) {
        sheet->SetCell(pos, text);
        {
            CellInterface* cell = sheet->GetCell(pos);
            ASSERT(cell != nullptr);
            std::cout << cell->GetText() << std::endl;
            std::cout << std::get<double>(cell->GetValue()) << std::endl;
        } {
            const auto& sheet_c = sheet;
            CellInterface* cell = sheet_c->GetCell(pos);
            ASSERT(cell != nullptr);
            std::cout << cell->GetText() << std::endl;
            std::cout << std::get<double>(cell->GetValue()) << std::endl;
        }
    };

    checkCell("A1"_pos, "=1");
    checkCell("B2"_pos, "=1/2");
    checkCell("A3"_pos, "=(1+1)/-1");
    checkCell("C3"_pos, "=(1+1)/(+1)");

    checkCell("A2"_pos, "=A1");
    checkCell("B3"_pos, "=B2+(12/3 - 2)");
    checkCell("A4"_pos, "=A3+C3");
    checkCell("C4"_pos, "=C3 + B2 / C3");
    /*
    auto v = sheet->GetCell("C4"_pos)->GetReferencedCells();
    for (auto x : v){
        std::cout << x << ' ';
    }
    std::cout << std::endl;
    */
    checkCell("D1"_pos, "=A1 + A1");
}

void PrintSheet(const std::unique_ptr<SheetInterface>& sheet) {
    std::cout << sheet->GetPrintableSize() << std::endl;
    sheet->PrintTexts(std::cout);
    std::cout << std::endl;
    sheet->PrintValues(std::cout);
    std::cout << std::endl;
}

void TestClearPrint() {
    auto sheet = CreateSheet();
    for (int i = 0; i <= 5; ++i) {
        sheet->SetCell(Position{i, i}, std::to_string(i));
    }

    sheet->ClearCell(Position{3, 3});

    for (int i = 5; i >= 0; --i) {
        sheet->ClearCell(Position{i, i});
        PrintSheet(sheet);
    }
}
}  // namespace

int main() {
    TestRunner tr;
    RUN_TEST(tr, TestPositionAndStringConversion);
    RUN_TEST(tr, TestPositionToStringInvalid);
    RUN_TEST(tr, TestStringToPositionInvalid);
    RUN_TEST(tr, TestEmpty);
    RUN_TEST(tr, TestInvalidPosition);
    RUN_TEST(tr, TestSetCellPlainText);
    RUN_TEST(tr, TestClearCell);
    RUN_TEST(tr, TestFormulaArithmetic);
    RUN_TEST(tr, TestFormulaReferences);
    RUN_TEST(tr, TestFormulaExpressionFormatting);
    RUN_TEST(tr, TestFormulaReferencedCells);
    RUN_TEST(tr, TestErrorValue);
    RUN_TEST(tr, TestErrorArithmetic);
    RUN_TEST(tr, TestEmptyCellTreatedAsZero);
    RUN_TEST(tr, TestFormulaInvalidPosition);
    RUN_TEST(tr, TestPrint);
    RUN_TEST(tr, TestCellReferences);
    RUN_TEST(tr, TestFormulaIncorrect);
    RUN_TEST(tr, TestCellCircularReferences);
    RUN_TEST(tr, TestSetGetCellCellRef);
    //TestClearPrint();
    //std::cout << "(5, 5)\n0\n\t1\n\t\t2\n\n\t\t\t\t4\n\n0\n\t1\n\t\t2\n\n\t\t\t\t4\n\n(3, 3)\n0\n\t1\n\t\t2\n\n0\n\t1\n\t\t2\n\n(3, 3)\n0\n\t1\n\t\t2\n\n0\n\t1\n\t\t2\n\n(2, 2)\n0\n\t1\n\n0\n\t1\n\n(1, 1)\n0\n\n0\n\n(0, 0)\n\n" << std::endl;
}   
