#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <variant>

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
    CheckValid(pos);
    if (!sheet_.count(pos) || sheet_.at(pos) == nullptr){
        try {
            sheet_[pos] = std::make_unique<Cell>(text,this,pos);
        } catch (const CircularDependencyException& e) {
            sheet_.erase(pos);
            throw e;
        }
        for (auto pos_ : sheet_[pos]->GetReferencedCells()){
            auto set_cells = GetRawCell(pos_)->GetDependentCells();
            set_cells.insert(dynamic_cast<Cell*>(sheet_[pos].get()));
            GetRawCell(pos_)->SetDependentCells(set_cells);
        }
    }
    else {
        if (sheet_.at(pos)->GetText() == text){
            return;
        }
        GetRawCell(pos)->InvalidateCache();
        auto dep_cells = dynamic_cast<Cell*>(sheet_[pos].get())->GetDependentCells();
        auto old_text = sheet_[pos]->GetText();
        sheet_[pos].reset();
        try {
            sheet_[pos] = std::make_unique<Cell>(text,this,pos);           
        } catch (const CircularDependencyException& e){
            sheet_[pos] = std::make_unique<Cell>(old_text,this,pos);
            throw e;
        }
        dynamic_cast<Cell*>(sheet_[pos].get())->SetDependentCells(dep_cells);
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    CheckValid(pos);
    if (sheet_.count(pos) && sheet_.at(pos)!=nullptr /*&& sheet_.at(pos).get()->GetText() != ""*/){
        return sheet_.at(pos).get();
    }
    return nullptr;
    /*
    if (!sheet_.count(pos)){
        return nullptr;
    } else if (sheet_.at(pos)){
        if (sheet_.at(pos).get()->GetText() == ""){
            return nullptr;
        }
    } else {
        return nullptr;
    }
    return sheet_.at(pos).get();
    */
}
CellInterface* Sheet::GetCell(Position pos) {
    CheckValid(pos);
    if (sheet_.count(pos) && sheet_.at(pos)!=nullptr /*&& sheet_.at(pos).get()->GetText() != ""*/){
        return sheet_.at(pos).get();
    }
    return nullptr;
    /*
    if (!sheet_.count(pos)){
        return nullptr;
    } else if (sheet_.at(pos)){
        if (sheet_.at(pos).get()->GetText() == ""){
            return nullptr;
        }
    } else {
        return nullptr;
    }
    return sheet_.at(pos).get();
    */
}

Cell* Sheet::GetRawCell(Position pos) {
    CheckValid(pos);
    if (!sheet_.count(pos)){
        sheet_[pos] = std::make_unique<Cell>("",this,pos);
    }
    return dynamic_cast<Cell*>(sheet_[pos].get());
}

void Sheet::ClearCell(Position pos) {
    CheckValid(pos);
    if (sheet_.count(pos)){
        GetRawCell(pos)->InvalidateCache();  
        for (auto pos_ : sheet_[pos]->GetReferencedCells()){
            auto set_cells = GetRawCell(pos_)->GetDependentCells();
            set_cells.erase(dynamic_cast<Cell*>(sheet_[pos].get()));
            GetRawCell(pos_)->SetDependentCells(set_cells);
        }
        sheet_[pos].reset();
        sheet_.erase(pos);
    }
    /*
    if (sheet_.count(pos)){
        auto dep_cells = dynamic_cast<Cell*>(sheet_[pos].get())->GetDependentCells();
        sheet_[pos].reset();
        sheet_[pos] = std::make_unique<Cell>("",this,pos);
        dynamic_cast<Cell*>(sheet_[pos].get())->SetDependentCells(dep_cells);
    }
    */
}

Size Sheet::GetPrintableSize() const {
    if (sheet_.empty()){
        return {0,0};
    }
    int max_row = 0;
    int max_col = 0;
    bool flag = false;
    for (const auto& [pos,cell] : sheet_){
        if (cell->GetText()!= ""){
            flag = true;
            max_row = std::max(max_row,pos.row);
            max_col = std::max(max_col,pos.col);
        }
    }
    if (!flag) {
        return {0,0};
    }
    return {max_row+1,max_col+1};
}

void Sheet::PrintValues(std::ostream& output) const {
    Size max_size = GetPrintableSize();
    for (int i = 0; i < max_size.rows; ++i){
        for (int j = 0; j < max_size.cols; ++j){
            if (GetCell({i,j}) != nullptr){ 
                auto val = GetCell({i,j})->GetValue();
                if (std::holds_alternative<FormulaError>(val)){
                    output << std::get<FormulaError>(val); 
                } else if (std::holds_alternative<double>(val)) {
                    output << std::get<double>(val);
                } else if (std::holds_alternative<std::string>(val)) {
                    output << std::get<std::string>(val);
                }
            }
            if (j == max_size.cols-1){
                output << '\n';
            }
            else {
                output << '\t';
            }
        }
    }
}
void Sheet::PrintTexts(std::ostream& output) const {
    Size max_size = GetPrintableSize();
    for (int i = 0; i < max_size.rows; ++i){
        for (int j = 0; j < max_size.cols; ++j){
            if (GetCell({i,j}) != nullptr){ 
                output << GetCell({i,j})->GetText();
            }
            if (j == max_size.cols-1){
                output << '\n';
            }
            else {
                output << '\t';
            }
        }
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}

void Sheet::CheckValid(Position pos) const {
    if (!pos.IsValid()){
        throw InvalidPositionException("");
    }
}