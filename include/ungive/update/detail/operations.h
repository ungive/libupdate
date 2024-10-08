#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "ungive/update/detail/log.h"
#include "ungive/update/internal/types.h"
#include "ungive/update/internal/win/startmenu.h"
#include "ungive/update/internal/zip.h"

namespace ungive::update::operations
{

class flatten_extracted_directory : public types::content_operation
{
public:
    // Flattens the extracted directory if the ZIP file contains one directory.
    flatten_extracted_directory() {}

    void operator()(std::filesystem::path const& extracted_directory) override
    {
        if (!internal::flatten_root_directory(extracted_directory.string())) {
            throw std::runtime_error(
                "failed to flatten extracted root directory");
        }
    }
};

class create_start_menu_shortcut : public types::content_operation
{
public:
    // Installs a start menu shortcut to the target executable.
    // If the target executable path is a relative path,
    // the executable path is resolved to be within the directory
    // of the extracted and installed update.
    // The category name is optional and resembles a subfolder
    // in which the shortcut is placed.
    // Optionally, the start menu shortcut can only be updated,
    // if it exists, and otherwise not be created.
    create_start_menu_shortcut(std::filesystem::path const& target_executable,
        std::filesystem::path const& link_name,
        std::optional<std::filesystem::path> const& category_name =
            std::nullopt,
        bool only_update = false)
        : m_target_executable{ target_executable }, m_link_name{ link_name },
          m_category_name{ category_name }, m_only_update{ only_update }
    {
        if (link_name.empty()) {
            throw std::invalid_argument("the link name cannot be empty");
        }
        if (link_name.has_parent_path()) {
            throw std::invalid_argument(
                "the start menu entry link name cannot have a parent path");
        }
        if (category_name.has_value()) {
            if (category_name->empty()) {
                throw std::invalid_argument(
                    "the category name cannot be empty");
            }
            if (category_name->has_parent_path()) {
                throw std::invalid_argument(
                    "the start menu entry category cannot have a parent path");
            }
        }
    }

    void operator()(std::filesystem::path const& extracted_directory) override
    {
        auto target_executable = m_target_executable;
        if (target_executable.is_relative()) {
            target_executable = extracted_directory / target_executable;
        }
        if (m_only_update &&
            !internal::win::has_start_menu_entry(target_executable, m_link_name,
                m_category_name.value_or(L""))) {
            // Start menu entry should only be updated but does not exist.
            return;
        }
        if (!std::filesystem::exists(target_executable)) {
            throw std::runtime_error(
                "the start menu shortcut target executable does not exist: " +
                target_executable.string());
        }
        auto result = internal::win::create_start_menu_entry(
            target_executable, m_link_name, m_category_name.value_or(L""));
        if (!result) {
            throw std::runtime_error("failed to create start menu shortcut");
        }
    }

private:
    std::filesystem::path m_target_executable;
    std::filesystem::path m_link_name;
    std::optional<std::filesystem::path> m_category_name;
    bool m_only_update;
};

class update_start_menu_shortcut : public create_start_menu_shortcut
{
public:
    // Works the same as create_start_menu_shortcut,
    // but only updates the shortcut if it already exists.
    update_start_menu_shortcut(std::filesystem::path const& target_executable,
        std::filesystem::path const& link_name,
        std::optional<std::filesystem::path> const& category_name =
            std::nullopt)
        : create_start_menu_shortcut(
              target_executable, link_name, category_name, true)
    {
    }
};

class ignore_failure : public types::content_operation
{
public:
    // Wrapper for a content operation to ignore any errors that might occur.
    template <typename O,
        typename std::enable_if<std::is_base_of<types::content_operation,
            O>::value>::type* = nullptr>
    ignore_failure(O const& operation) : m_content_operation{ operation }
    {
    }

    void operator()(std::filesystem::path const& extracted_directory) override
    {
        try {
            m_content_operation(extracted_directory);
        }
        catch (std::exception const& e) {
            logger()(log_level::warning,
                std::string("content operation skipped with error: ") +
                    e.what());
            return;
        }
    }

private:
    internal::types::content_operation_func m_content_operation;
};

} // namespace ungive::update::operations
