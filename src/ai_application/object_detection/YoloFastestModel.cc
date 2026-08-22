/*
 * SPDX-FileCopyrightText: Copyright 2022 Arm Limited and/or its affiliates <open-source-office@arm.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "YoloFastestModel.hpp"

#include "log_macros.h"

const tflite::MicroOpResolver& arm::app::YoloFastestModel::GetOpResolver()
{
    return this->m_opResolver;
}

bool arm::app::YoloFastestModel::EnlistOperations()
{
    this->m_opResolver.AddDepthwiseConv2D();
    this->m_opResolver.AddConv2D();
    this->m_opResolver.AddAdd();
    this->m_opResolver.AddResizeNearestNeighbor();
    /*These are needed for UT to work, not needed on FVP */
    this->m_opResolver.AddPad();
    this->m_opResolver.AddMaxPool2D();
    this->m_opResolver.AddConcatenation();
    /* NOTE: pushed models may only use ops linked in here. Both shipped
     * model families are 100% NPU-resident after vela (single ethos-u op),
     * so no extra CPU kernels are linked - MRAM is within ~1 KB of full.
     * A pushed model with CPU-fallback ops is rejected at Init with a
     * clear "failed to get registration" log. */
    this->m_opResolver.AddAveragePool2D();
    this->m_opResolver.AddReshape();

#if 1 /* ARM_NPU: Ethos-U always present on RA8P1 */
    if (kTfLiteOk == this->m_opResolver.AddEthosU()) {
        info("Added ethos-u support to op resolver\n");
    } else {
        printf_err("Failed to add Arm NPU support to op resolver.");
        return false;
    }
#endif /* ARM_NPU */
    return true;
}
