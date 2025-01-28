/*
 * SPDX-FileCopyrightText: (C) 2025 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RETURNCODES_H
#define RETURNCODES_H

enum class RC : int {
    OK = 0,
    FileSystemError = 1,
    InvalidConfig = 6
};

#endif // RETURNCODES_H
