// Copyright (c) 2026 ReconDuck contributors
// Licensed under the MIT License, see LICENSE.md.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ManualLaunch
{
// Paths are canonical, case-folded Windows paths supplied by the caller.
struct Process
{
  uint32_t pid;
  uint32_t parent;
  uint64_t started;
  const wchar_t *path;
};

inline bool IsExactTarget(const Process &process, const wchar_t *target)
{
  const wchar_t *path = process.path;
  if(!path || !*path || !target || !*target)
    return false;
  while(*path && *path == *target)
  {
    path++;
    target++;
  }
  return *path == *target;
}

inline bool IsNew(const Process &process, const Process *existing, size_t count)
{
  if(process.pid == 0)
    return false;
  for(size_t i = 0; i < count; i++)
    if(existing[i].pid == process.pid)
      return process.started != 0 && existing[i].started != 0 &&
             process.started != existing[i].started;
  return true;
}

inline bool HasTargetAncestor(const Process &process, const Process *current, size_t count,
                              const Process *existing, size_t existingCount, const wchar_t *target)
{
  const Process *child = &process;
  // Bound traversal even for stale or cyclic process snapshots.
  for(size_t depth = 0; depth < count && depth < 64; depth++)
  {
    const Process *parent = NULL;
    for(size_t i = 0; i < count; i++)
      if(current[i].pid == child->parent)
        parent = &current[i];
    if(!parent || !IsNew(*parent, existing, existingCount) || !parent->started ||
       !child->started || parent->started > child->started)
      return false;
    if(IsExactTarget(*parent, target))
      return true;
    child = parent;
  }
  return false;
}

inline bool CanConnect(bool exactTarget, bool child, bool supported, bool presenting)
{
  return exactTarget || (child && supported && presenting);
}

inline bool CanInject(const Process &process, const Process *existing, size_t count,
                      const wchar_t *target, unsigned attempts, bool cancelled)
{
  return !cancelled && attempts < 3 && process.started != 0 && IsNew(process, existing, count) &&
         IsExactTarget(process, target);
}
};
