// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Plenteum Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <walletbackend/WalletBackend.h>

void optimize(const std::shared_ptr<WalletBackend> walletBackend);

bool optimizeRound(const std::shared_ptr<WalletBackend> walletBackend);
