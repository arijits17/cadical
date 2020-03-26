#include "internal.hpp"

namespace CaDiCaL {

// This function determines the next decision variable on the queue, without
// actually removing it from the decision queue, e.g., calling it multiple
// times without any assignment will return the same result.  This is of
// course used below in 'decide' but also in 'reuse_trail' to determine the
// largest decision level to backtrack to during 'restart' without changing
// the assigned variables (if 'opts.restartreusetrail' is non-zero).

int Internal::next_decision_variable_on_queue () {
  int64_t searched = 0;
  int res = queue.unassigned;
  while (val (res))
    res = link (res).prev, searched++;
  if (searched) {
    stats.searched += searched;
    update_queue_unassigned (res);
  }
  LOG ("next queue decision variable %d bumped %" PRId64 "", res, bumped (res));
  return res;
}

// This function determines the best decision with respect to score.
//
int Internal::next_decision_variable_with_best_score () {
  int res = 0;
  for (;;) {
    res = scores.front ();
    if (!val (res)) break;
    (void) scores.pop_front ();
  }
  LOG ("next decision variable %d with score %g", res, score (res));
  return res;
}

int Internal::next_decision_variable () {
  if (use_scores ()) return next_decision_variable_with_best_score ();
  else               return next_decision_variable_on_queue ();
}

/*------------------------------------------------------------------------*/


// Implements LSIDS based phase selection
// Checks the score for both the literals of the variable, returns the higher

int Internal::select_lsids_based_phase (int idx) {

  assert (idx <= max_var);
  unsigned int pos = 2u * idx ;
  unsigned int neg = 2u * idx + 1 ;

  if (lstab[pos] > lstab[neg])
      return 1;
  else
      return -1;
}

/*------------------------------------------------------------------------*/

int Internal::decide_cbt_phase (int idx, bool target) {

  int phase = 0;
  int heuristic = opts.chronophase;
  const int initial_phase = opts.phase ? 1 : -1;

  Random random (opts.seed);

  if (heuristic == 1) phase = select_lsids_based_phase(idx);
  if (heuristic == 2) phase = random.generate_bool () ? -1 : 1;
  if (heuristic == 3) phase = -1;
  if (heuristic == 4) phase = phases.target[idx];
  if (heuristic == 5) phase = phases.best[idx];
  if (heuristic == 6) phase = phases.prev[idx];
  if (heuristic == 7) phase = phases.min[idx];

  if (!phase) phase = initial_phase;

  return phase * idx;
}

/*------------------------------------------------------------------------*/

// Implements phase saving as well using a target phase during
// stabilization unless decision phase is forced to the initial value.

int Internal::decide_phase (int idx, bool target) {
  const int initial_phase = opts.phase ? 1 : -1;
  int phase = 0;
  if (cbt && opts.chronophase > 0) return decide_cbt_phase(idx, target);
  if (force_saved_phase) phase = phases.saved[idx];
  if (!phase && opts.forcephase) phase = initial_phase;
  if (!phase && target)  phase = phases.target[idx];
  if (!phase) phase = phases.saved[idx];
  if (!phase) phase = initial_phase;
  return phase * idx;
}

// The likely phase of an variable used in 'collect' for optimizing
// co-location of clauses likely accessed together during search.

int Internal::likely_phase (int idx) { return decide_phase (idx, false); }

/*------------------------------------------------------------------------*/

bool Internal::satisfied () {
  size_t assigned = trail.size ();
  if (propagated < assigned) return false;
  if ((size_t) level < assumptions.size ()) return false;
  return (assigned == (size_t) max_var);
}

// Search for the next decision and assign it to the saved phase.  Requires
// that not all variables are assigned.

int Internal::decide () {
  assert (!satisfied ());
  START (decide);
  int res = 0;
  if ((size_t) level < assumptions.size ()) {
    const int lit = assumptions[level];
    assert (assumed (lit));
    const signed char tmp = val (lit);
    if (tmp < 0) {
      LOG ("assumption %d falsified", lit);
      failing ();
      res = 20;
    } else if (tmp > 0) {
      LOG ("assumption %d already satisfied", lit);
      level++;
      control.push_back (Level (0, trail.size ()));
      LOG ("added pseudo decision level");
    } else {
      LOG ("deciding assumption %d", lit);
      search_assume_decision (lit);
    }
  } else {
    stats.decisions++;
    int idx = next_decision_variable ();
    const bool target = opts.stabilizephase && stable;
    int decision = decide_phase (idx, target);
    search_assume_decision (decision);
  }
  STOP (decide);
  return res;
}

}
