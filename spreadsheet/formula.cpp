#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    output << "#" << fe.ToString() << "!";
    return output;
}

std::string_view FormulaError::ToString() const {
    switch (category_)
    {
    case FormulaError::Category::Arithmetic:
        return "ARITHM";
        break;
    case FormulaError::Category::Ref:
        return "REF";
        break;
    case FormulaError::Category::Value:
        return "VALUE";
        break;
    }
    return "ARITHM";
}

namespace {
class Formula : public FormulaInterface {
public:
    // Реализуйте следующие методы:
    explicit Formula(std::string expression)
    try : ast_(ParseFormulaAST(expression))
    { 
        //ast_.PrintFormula(std::cout);
    }
    catch (...)
    {
        throw FormulaException("Parsing Error");
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        Value output;
        try{
            output = ast_.Execute(sheet);
        } catch(const FormulaError& e) {
            return e;
        } catch(...){
            throw FormulaException("");
        }
        return output;
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const {
        return ast_.GetReferencedCells();
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}