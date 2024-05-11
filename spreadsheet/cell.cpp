#include "cell.h"

#include <cassert>
#include <string>
#include <optional>
#include <sstream>
#include <algorithm>
#include <queue>

// Конструктор и деструкор

Cell::Cell(const std::string& text, SheetInterface* sheet, Position pos)
    : sheet_(sheet) {
    if (text.empty()){
        type_ = Type::EMPTY;
        impl_= std::make_unique<EmptyImpl>();
    }else if (text[0] == FORMULA_SIGN){
        if (text.size() == 1) {
            type_ = Type::TEXT;
            impl_= std::make_unique<TextImpl>(text);
        } else {
            type_ = Type::FORMULA;
            impl_= std::make_unique<FormulaImpl>(text.substr(1,text.size()-1));
        }
    } else {
        type_ = Type::TEXT;
        impl_= std::make_unique<TextImpl>(text);
    }
    // Проверка на циклические зависимости
    if (type_ == Type::FORMULA){
        FormulaImpl* formula_impl = dynamic_cast<FormulaImpl*>(impl_.get());
        std::queue<Position> queue;
        for (auto p : formula_impl->GetReferencedCells()){
            queue.push(p);
        }
        std::set<Position> predecessors;
        while (!queue.empty()) {
            Position current = queue.front();
            if (current == pos) {
                throw CircularDependencyException("");
            }
            queue.pop();
            auto cell = sheet_->GetCell(current);
            if (cell != nullptr){
                auto ref_cells = cell->GetReferencedCells();
                for (Position x : ref_cells) {
                    if (!predecessors.count(x)) {
                        queue.push(x);
                    }
                }
            } else {
                sheet_->SetCell(current,"");
            }
            predecessors.insert(current);
        }

    }
}

Cell::~Cell() { 
    /*
        if (type_ == Type::FORMULA){
        FormulaImpl* formula_impl = dynamic_cast<FormulaImpl*>(impl_.get());
        if (sheet_ != nullptr){
            for (auto pos_ : formula_impl->GetReferencedCells()){
                try {
                    CellInterface* cell = sheet_->GetCell(pos_);
                    if (cell != nullptr){
                        auto set_cells = dynamic_cast<Cell*>(cell)->GetDependentCells();
                        set_cells.erase(this);
                        dynamic_cast<Cell*>(sheet_->GetCell(pos_))->SetDependentCells(set_cells);
                    }
                } catch(...){
                    continue;
                }
            }
        }
    }
    */
}

// Инвалидация кэша

void Cell::InvalidateCache() {
    if (type_ == Type::FORMULA){
        FormulaImpl* formula_impl = dynamic_cast<FormulaImpl*>(impl_.get());
        formula_impl ->InvalidateCache();
    }
    for (auto cell : dependent_cells_){
        if (cell != nullptr){
            cell->InvalidateCache();
        }    
    }
}

// Получение значений

Cell::Value Cell::GetValue() const {
    return impl_->GetValue(*sheet_);
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {   
    if (type_ == Type::FORMULA){
        FormulaImpl* formula_impl = dynamic_cast<FormulaImpl*>(impl_.get());
        return formula_impl->GetReferencedCells();
    }
    return {};
}

void Cell::SetDependentCells(const std::set<Cell*>& cells) {
    dependent_cells_ = cells;
}

std::set<Cell*> Cell::GetDependentCells() const {
    return dependent_cells_;
}

// Методы имплементаций
    
// Получение значений

Cell::Value Cell::TextImpl::GetValue([[maybe_unused]] const SheetInterface& sheet) const {
    if (isdigit(text_[0])){
        size_t pos;
        double num = stod(text_,&pos);
        if (pos == text_.size()){
            return num;
        }
    }
    if (text_[0] == ESCAPE_SIGN) {
        return text_.substr(1,text_.size()-1);
    }
    return text_;
}

std::string Cell::TextImpl::GetText() const {
    return text_;
}

Cell::Value Cell::FormulaImpl::GetValue([[maybe_unused]] const SheetInterface& sheet) const {
    Value result;
    if (cache_.has_value()){
        return cache_.value();
    }
    FormulaInterface::Value val = formula_->Evaluate(sheet);
    if (std::holds_alternative<FormulaError>(val)){
        result = std::get<FormulaError>(val); 
    } else if (std::holds_alternative<double>(val)) {
        result = std::get<double>(val);
    }
    cache_ = result;
    return result;
}

std::string Cell::FormulaImpl::GetText() const {
    return FORMULA_SIGN + formula_->GetExpression();
}

Cell::Value Cell::EmptyImpl::GetValue([[maybe_unused]] const SheetInterface& sheet) const {
    return 0.0;
}

std::string Cell::EmptyImpl::GetText() const {
    return "";
}

std::vector<Position> Cell::FormulaImpl::GetReferencedCells() const {
    return formula_->GetReferencedCells();
}

// Инвалидация кэша

void Cell::FormulaImpl::InvalidateCache() {
    cache_.reset();
}