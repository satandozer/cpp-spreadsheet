#pragma once

#include "common.h"
#include "formula.h"
#include <unordered_set>
#include <optional>
#include <set>
#include <iostream>



class Cell : public CellInterface {
    enum Type {
        EMPTY,
        TEXT,
        FORMULA
    };
public:
    explicit Cell(const std::string& text, SheetInterface* sheet, Position pos);
    ~Cell();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    void InvalidateCache();
    void SetDependentCells(const std::set<Cell*>& cells);
    std::set<Cell*> GetDependentCells() const;
private:

    // Базовый класс имплементации
    class Impl {
        public:
            virtual Value GetValue([[maybe_unused]] const SheetInterface& sheet) const = 0;
            virtual std::string GetText() const = 0;
    };

    // Имплементация текстовой ячейки
    class TextImpl : public Impl {
        public:
            TextImpl(const std::string& text)
                : text_(text) {}
            Value GetValue([[maybe_unused]] const SheetInterface& sheet) const override;
            std::string GetText() const override;
        private:
            std::string text_ = "";
    };

    // Имплементация формульной ячейки
    class FormulaImpl : public Impl {
        public:
            FormulaImpl(const std::string& formula) 
                : text_(formula){
                    try {
                        formula_ = ParseFormula(formula);
                    } catch (const FormulaException& e){
                        throw e;
                    }
                }
            Value GetValue([[maybe_unused]] const SheetInterface& sheet) const override;
            std::string GetText() const override;

            std::vector<Position> GetReferencedCells() const;
            void InvalidateCache();

        private:
            std::string text_ = "";
            std::unique_ptr<FormulaInterface> formula_;

            // Кэшированное значение ячейки
            mutable std::optional<Value> cache_;
    };

    // Имплементация пустой ячейки
    class EmptyImpl : public Impl {
        public:
            Value GetValue([[maybe_unused]] const SheetInterface& sheet) const override;
            std::string GetText() const override;
    };

    std::unique_ptr<Impl> impl_;

    mutable SheetInterface* sheet_;

    // Зависимые ячейки, от текущей
    std::set<Cell*> dependent_cells_;
    Cell::Type type_;
};