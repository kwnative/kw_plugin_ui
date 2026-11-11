/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Build-only syntax check of the optional provider's public contract.
 * Never linked into the game, used as a provider or executed at runtime. */
#include <kwthreading/integration/presentation_worker.h>
int kwui_threading_contract_check(void)
{
    struct kwthreading_presentation_task_dependencies submission = {{sizeof(submission), KWTHREADING_PRESENTATION_FXAA, 0, 0, 0, 0},
                                                                    KWTHREADING_PRESENTATION_NO_VERTEX_INDEX_ACCESS};
    return submission.task.size == sizeof(submission);
}
