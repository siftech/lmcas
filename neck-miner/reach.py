#! /usr/bin/env python3

import os
import copy
from pprint import pprint, pformat
from collections import deque

# BEGIN Globals (Figure out a decent way to represent this in C++)

# DONE in C++
# Functions associated with properties when we summarize the contribution of
# a single exit path.
policy_summarize_exit_path = {
    "numSucceededLoopHeads": lambda x, y: y if x is None else x + y,
    "numSucceededGAF": lambda x, y: y if x is None else x + y,
}

# DONE in C++
# Functions associated with properties when we summarize all of the
# contributions of a set of exit paths.
policy_merge_exit_path_summaries = {
    "numSucceededLoopHeads": lambda x, y: y if x is None else max(x, y),
    "numSucceededGAF": lambda x, y: y if x is None else max(x, y),
}

# DONE in C++
# When we're accumulating a callee's contributions into the basic block that
# called it, use this set of functions associated with properties.
policy_merge_basic_block_contribution = {
    "numSucceededLoopHeads": lambda x, y: y if x is None else x + y,
    "numSucceededGAF": lambda x, y: y if x is None else x + y,
}
# END Globals


# DONE in C++
# Utility Function
# Lookup the propName policy function in policy and apply it to the dst and src
# property key value and store in dst. This side effects into dst and also
# returns the new value.
# dst and src are hash tables, policy is a hash table of propname -> lambda
def apply_policy_to_property(dst, src, propName, policy):
    tmp = dst.get(propName)
    func = policy.get(propName)
    assert func is not None, "apply_policy_to_property(): Programmer error!"
    # NOTE: We could pass None as tmp here and it is expected that the
    # policy function deals with this case.
    dst[propName] = func(tmp, src[propName])
    return dst[propName]


# DONE in C++
# Utility Function
# Apply the policy to the src and dst hash tables.
def apply_policy(dst, src, policy):
    for propName in src:
        apply_policy_to_property(dst, src, propName, policy)
    return dst


# DONE in C++
# Take a single exit path and produce a contribution table
# Summarize Exit Path (Function local only)
#   1. Exit Path Summary Function.
#       (-> exit_path exit_path_contribution_ht)
def summarize_exit_path(b, funcname, exit_path, policy):
    accumulator = {}

    # We only want the static contribution of the the last node in the
    # basic block path....
    (bbname, idx, bb_summary_ht) = exit_path[-1]
    bb_static_contrib = b[funcname]["bbs"][bbname]["props"]
    apply_policy(accumulator, bb_static_contrib, policy)

    # Then, gather the callee_contributions for each BB to the start.
    # So it as a reduce from the right side of the list.
    for bbitem in reversed(exit_path):
        (bbitem_bbname, bbitem_idx, bbitem_contrib) = bbitem
        apply_policy(accumulator, bbitem_contrib, policy)

    return accumulator


# DONE in C++
# take a list of exit path contributions and return a single summary table
# Merge Exit Path Summaries (Function local only)
#   2. Exit Summary Merge Function.
#       (-> exit_path_contribution_ht_list final_contribution_ht)
def merge_exit_path_summaries(b, exit_path_contribs, policy, startleft=True):
    final_contribution = {}

    epcs = iter(exit_path_contribs) if startleft else reversed(exit_path_contribs)

    for epc in epcs:
        apply_policy(final_contribution, epc, policy)

    return final_contribution


# DONE in C++
# Side effect into the summarization table the merging of the summary
# table by the policy. Also return the modified summarization table.
# Merge Basic Block Contribution
#   3. Basic Block Summerize a Summary Function.
#       (-> summarization_ht summary_ht summarization_ht)
def merge_basic_block_contribution(b, summarization, summary, policy):
    apply_policy(summarization, summary, policy)
    return summarization


# DONE in C++
# Walks the path from the leaf to the root and produces a summary of the
# attributes of that path according to the policy given.
def summarize_complete_path(b, fpath):
    accumulate = {}
    contribs = []

    # Treat each portion as an exit path and compute the contrbution of it.
    # These can happen in any order.
    for funcname, fpath_segment in fpath:
        contribs.append(
            summarize_exit_path(b, funcname, fpath_segment, policy_summarize_exit_path)
        )

    # Then, we accumulate the result from leaf to root.
    for contrib in reversed(contribs):
        apply_policy(accumulate, contrib, policy_merge_basic_block_contribution)

    return accumulate


# A basic block BFS queue state for an arbitrary function's BB graph.
class BB_BFS_Q:

    # Keep synthesized attributes for basic blocks which represent
    # features found when exploring functions called in those basic
    # blocks. For example, if I cal a function and it passed a loop, then
    # the basic block which called it should be notated that yes, it passed
    # a loop too!

    # DONE in C++
    def __init__(self, funcName, bb_start, choke_point_forbidden, logp=True):
        self.debug = logp
        self.choke_point_forbidden = choke_point_forbidden
        self.funcName = funcName
        self.queue = deque()
        self.queue.append(bb_start)
        self.observed = {bb_start: True}
        self.parents = {bb_start: None}
        self.visiting = None
        self.visiting_inst_processed = False
        self.chokep = False

        # when we yield for a recursion into a function
        # This allows us to continue if we didn't like that path.
        self.yielded = False
        self.yielded_future = None
        self.yield_at_visiting = None
        self.yield_at_callee_funcName = None
        self.yield_at_inst_idx = None

        # A list of exit paths. These are deep copied function scoped
        # paths in the order of BFS to every basic block with no children,
        # which are usually return or exit points in the function.
        self.exit_paths = []

        # For each BB in this function's BB_BFS_Q, we keep a set of attributes
        # that represent one or more collected summaries of callee function
        # contribution called inside of that basic block.  These will show up
        # in the BB entries in the fpath object and are specific to the path.
        #
        # Key: BB in this function,
        # Value: hash table with
        #        Key: cp_propname
        #        Value: Prop value
        self.callee_summaries = dict()

    # DONE in C++
    def log(self, msg):
        if not self.debug:
            return
        print(msg)

    # DONE in C++
    def __len__(self):
        return len(self.queue)

    # DONE in C++
    def finished(self):
        assert not (self.yielded and self.visiting_inst_processed), (
            "This is a broken state. Can't be yielded and all instructions "
            "were processed for a basic block."
        )

        return (
            not self.yielded and len(self.queue) == 0 and self.visiting_inst_processed
        )

    # ---------------------------------------------------------

    # DONE in C++
    # Iterate over the exit_paths and compute a final contribution table
    def compute_final_contribution(self, b):
        results = []
        for ep in self.exit_paths:
            contrib = summarize_exit_path(
                b, self.funcName, ep, policy_summarize_exit_path
            )
            results.append(contrib)

        final_contribution = merge_exit_path_summaries(
            b, results, policy_merge_exit_path_summaries
        )

        return final_contribution

    # DONE in C++
    # We can check to see if there are summarizing attributes for this specific
    # basic block. If not, return an empty hash table.
    def get_callee_summary(self, bb):
        csum = self.callee_summaries.get(bb)
        if csum is None:
            csum = dict()
            self.callee_summaries[bb] = csum
        return csum

    # DONE in C++
    # NOTE: This is ONLY called by the USER of BB_BFS_Q in order to summarize
    # the props of an entire function call both into the choke point BB's
    # summarization props from where it was called, and also into the
    # collective summary we're keeping.
    def accumulate_callee_contribution(self, b, final_contrib):
        # We accumulate the contribution INTO the visiting basic block's
        # callee_summaries. Get a reference to the callee summary.
        csum = self.get_callee_summary(self.visiting)

        # side effect into the reference csum, which is stored in the
        # callee_summaries.
        merge_basic_block_contribution(
            b, csum, final_contrib, policy_merge_basic_block_contribution
        )

    # ---------------------------------------------------------

    # NOTE: We can only visit if we're not yielded and assert over it.
    # NOTE: We can only currently visit a choke point ONCE, and only at
    # the very start of it before we expand instructions looking for
    # acceptable call sites to other functions.
    # DONE in C++
    def visit(self, b):
        assert not self.yielded, "Programmer Error: Cannot visit() while yielded!"

        self.visiting = self.queue.popleft()

        # Earliest detection of possible choke point BEFORE we start
        # processing the instructions in expand().
        self.chokep = b[self.funcName]["bbs"][self.visiting]["chokep"]

        self.visiting_inst_processed = False

        # self.log(f"/// before visit callback: {self.funcName}@{self.visiting}")

        return (self.visiting, self.chokep)

    # TODO: Validate that the proper basic blocks have the proper
    # summarization information related to them.
    # DONE in C++
    def current_path(self):
        if self.visiting is None:
            return []

        # NOTE: We can only yielded at a choke point.
        csum = self.get_callee_summary(self.visiting)
        if self.yielded:
            # tuple of basic block and instruction idx yielded at or None
            path = [(self.visiting, self.yield_at_inst_idx, csum)]
        else:
            path = [(self.visiting, None, csum)]

        parent = self.parents[self.visiting]
        while parent != None:
            path.append((parent, None, self.get_callee_summary(parent)))
            parent = self.parents[parent]
        path.reverse()
        return path

    def expand(self, b, p):
        if self.finished():
            return ["expanded", None, []]

        if self.yielded:
            self.log(f"  BB_BFS_Q: expand(): RESUME " f"self.visiting: {self.visiting}")
            # We're restarting from a previous yield.
            future_work = self.yielded_future
            self.yielded = False
            self.yielded_future = None
        else:
            assert (
                self.yielded_future == None
            ), "Can't have a yielded future while not yielded!"

            self.log(f"  BB_BFS_Q: expand(): START " f"self.visiting: {self.visiting}")
            insts = b[self.funcName]["bbs"][self.visiting]["inst"]
            # In the C++, prolly store this as the concrete list I'm
            # processing.
            future_work = enumerate(insts)
            self.yield_at_callee_funcName = None
            self.yield_at_inst_idx = -1

        # We can only yield if we find a participating function we want to call
        # in the instruction stream of the basic block.

        # TODO: The current feature set is implemented such that we can only
        # visit() at the start basic block. A basic block that is also
        # a choke point in which we did recurse, can not yet be split
        # into visitations of the basic block _at an instruction index_.

        # TODO: (DO LATER) It seems like to me, that when we visit what would
        # normally be a chokepoint, we actually secret it away, then visit
        # EVERYTHING else until we either reach another choke point (which is a
        # multi-neck situation!) or the open list becomes empty, in
        # which case, visiting the secreted away choke point allows us to
        # expand into the next part of the computation on the other side of the
        # choke point. The reason for this feature is we can both ensure that
        # all the aggregating info from functions calls in basic blocks are up
        # to date, but also we can detect multi-neck situations by enountering
        # a choke point in the queue when we alreasy have one (or more)
        # deferred ones.

        for idx, inst in future_work:
            # self.log(f"  BB_BFS_Q: expand(): "
            #    f"Proc inst: "
            #    f"F[{self.funcName}]@{self.visiting}:[{idx}] {inst}")

            if "call" not in inst:
                # Skip instructions which are not direct function calls
                continue

            callee_funcName = inst.split()[-1]

            if callee_funcName not in p:
                # Skip calls that are not in participating functions.
                self.log(
                    f"  BB_BFS_Q: expand(): "
                    f"NO_YIELD at "
                    f"F[{self.funcName}]@{self.visiting}:[{idx}] {inst}"
                )
                continue

            # We're about to enter into a new function so save state
            # and effect the yield.

            self.yield_at_visiting = self.visiting
            self.yield_at_callee_funcName = callee_funcName
            self.yield_at_inst_idx = idx
            self.yielded = True
            # The enumerator object stores its own recovery state,
            # so we can just pick it back up later.
            self.yielded_future = future_work
            self.log(
                f"  BB_BFS_Q: expand(): "
                f"YIELD at "
                f"F[{self.funcName}]@{self.visiting}:[{idx}] {inst}"
                f" | path-up-to-yield: OFF"
            )
            # f" | path-up-to-yield: OFF {self.current_path()}")
            return ["yield", callee_funcName, idx]

        assert (
            not self.visiting_inst_processed
        ), "Attempted to expand when already visited the instructions!"

        self.visiting_inst_processed = True

        # Now finally, when we can expand the visiting node into its children.

        succs = b[self.funcName]["bbs"][self.visiting]["succ"]
        expanded = []  # For debugging and verbose output
        if succs is not None:
            for succ in succs:
                if self.observed.get(succ) == None:
                    self.observed[succ] = True
                    self.parents[succ] = self.visiting
                    self.queue.append(succ)
                    expanded.append(succ)
            # self.log(f"  BB_BFS_Q: expand(): Expanded: {expanded}")
        else:
            # Save off in BFS order the current exit path to be summarized
            # later. NOTE: This path is _a_ path to this basic block, not
            # all paths to this no successor basic block.
            #
            # TODO: We probably want all paths from here back to the front
            # if the function and add all of those paths as exit paths.
            # This needs thinking about since capturing the path at the right
            # time is critical to make it work.
            self.exit_paths.append(copy.deepcopy(self.current_path()))

        self.log(
            f"  BB_BFS_Q: expand(): END self.visiting: {self.visiting}, "
            f"callee summary: {self.get_callee_summary(self.visiting)}"
        )
        return ["expanded", self.visiting, expanded]

    def __str__(self):
        return f"  BB_BFS_Q: funcName: {self.funcName} yield: {self.yielded} Q: {self.queue}"


class NeckSearch:
    # DONE in C++
    def __init__(self, p, b, start_func, logp=True):
        self.debug = logp
        self.p = p
        self.b = b
        self.S = []
        self.F = None
        self.start_func = start_func

        self.reinit()

    # DONE in C++
    def log(self, msg):
        if not self.debug:
            return
        print(msg)

    # DONE in C++
    # When called, reinitialize the state of the NeckSearch such that it
    # starts from the beginning again.
    def reinit(self):
        self.S = []
        self.F = BB_BFS_Q(self.start_func, self.b[self.start_func]["entry"], False)
        self.S.append(self.F)

    # DONE in C++
    # return True when there is no more work to do in searching.
    def finished(self):
        assert self.S[-1] is self.F, "Self.F is not the top of Self.S!"

        return len(self.S) == 1 and self.F.finished()

    # DONE in C++
    # Return a list of [func, [bb0, bb1, ..., bbn]] histories of the current
    # path.
    def get_complete_path(self):
        return [(bbq.funcName, bbq.current_path()) for bbq in self.S]

    # DONE in C++
    # Return a list of only the functions in the history of the search.
    def get_function_path(self):
        return [bbq.funcName for bbq in self.S]

    # DONE in C++
    # Perform one visit and process all yields until the next visit.
    # Pass the visited node, if it is a choke point, and the current
    # function path from start_func to the node to the visit_func.
    # Return a tuple where:
    #  first element is True if there are more nodes to expand else False,
    #  second value is if the visit func was actually called or not.
    #  third element is return value of VisitFunc or None if not called,
    # return value of the visit_func.
    def visit_next(self, visit_func):
        if len(self.F) == 0:
            # This should only happen if visit_next() was called on an already
            # finished NeckSearch. This is because the visit_next() will
            # automatically push the expand() until the last possible moment
            # and either we'll know we're done with the search, or know we can
            # visit_next() again.
            return (not self.finished(), False, None)

        # Otherwise, call the visit_func and expand until the search is found
        # to be completed, or we can call visit_next() again.

        # visit current node ONCE.
        (v, chokep) = self.F.visit(self.b)
        complete_path = self.get_complete_path()
        ret = visit_func(
            self.F.funcName, v, chokep, self.F.choke_point_forbidden, complete_path
        )
        # self.log(f"** Visit: F[{self.F.funcName}]@{v} Chokep: {chokep}")
        # self.log(f"   | Complete path: {complete_path}")

        # Purpose of this is to fully expand() until the only thing left to
        # do is visit (or ALL DONE)
        while True:
            outcome = self.F.expand(self.b, self.p)
            self.log(f"++ F[{self.F.funcName}] Outcome: {outcome}")

            if self.F.finished():
                if len(self.S) == 1:
                    break

                # Get contribution from called function.
                final_contrib = self.F.compute_final_contribution(self.b)
                # Back track to a previous function we're exploring...
                self.S.pop()  # get rid of myself at the top of S
                self.F = self.S[-1]  # F is now the new top.
                self.F.accumulate_callee_contribution(self.b, final_contrib)
                # Expand the less deep F we are coming back to.
                vnode = self.F.current_path()[-1][0]
                self.log(f"Backtracked to: F[{self.F.funcName}]@{vnode}")
                continue

            if self.F.yielded:
                self.log(
                    f"Yield: YIELD At Path: "
                    f"F[{self.F.funcName}]@{self.F.current_path()}"
                )
                fpath = self.get_function_path()
                # self.log(f"visit_next(): FPATH is {fpath}")
                callee_func = self.F.yield_at_callee_funcName
                inst_idx = self.F.yield_at_inst_idx

                if callee_func in fpath:
                    self.log("Yield: IGNORE Attempting recur. function path.")
                    continue

                # Looks like we can recurse into callee!
                entry = self.b[callee_func]["entry"]
                self.log(f"Yield: RECURSE into: " f"{callee_func}@{entry}[{inst_idx}]")

                # BIG NOTE: Recurse into to new F here!
                forbid_choke_points = not self.F.chokep or self.F.choke_point_forbidden
                # NOTE: self.F changes here to a NEW object!
                self.F = BB_BFS_Q(callee_func, entry, forbid_choke_points)
                # F will not be yielded here, we just made it.
                self.S.append(self.F)

                # The only left to do is visit() on the new self.F
                # so stop expanding.
                break

            # Here self.F is not yielded and not finished, so we can only
            # visit(), hence, we're done!
            break

        return (not self.finished(), True, ret)

    # DONE in C++
    # Continue to perform visits process all yields until there are no more
    # visits which will happen.
    # Pass the visited node, if it is a choke point, and the current
    # function path from start_func to the node to the visit_func.
    # If the visit_func returns True, keep going in the search. If it
    # returns False, immediately stop and return. If after this has happened,
    # visit_next() or visit_all() is called again, the remainder of the
    # work, if any, will be restarted.
    # Return True if there is more to visit, and False if done.
    def visit_all(self, visit_func):
        while not self.finished():
            (more_to_search, visit_func_called, user_continue) = self.visit_next(
                visit_func
            )
            if more_to_search == False or user_continue == False:
                return (more_to_search, user_continue)
        return (False, user_continue)


def test4(msg, p, b, start_func, verify=False, debug=True):
    print(f"\n\n\n\n-------")
    print(f"TEST 4: BEGIN {msg}")

    # key: func@basic_block,
    # value: is an incrementing value representing how many times we've
    # visited that node. Starts at 0.
    visit_count = {}

    def visit_func(func, node, chokep, forbidden, fpath):
        fpsum = summarize_complete_path(b, fpath)
        visit_key = f"{func}@{node}"

        # Ensure the current index exists.
        visit_count[visit_key] = visit_count.get(visit_key, 0)

        print(f"*** Visiting {func}@{node} : {visit_count[visit_key]}")
        print(f"     Chokep: {chokep}, Forbid: {forbidden}")
        print(f"     FPath Summary: {fpsum}")
        print(f"     Complete Path:")
        pprint(fpath)

        if verify:
            unit_test_data_for_visit = b[func]["bbs"][node]["unit-test-data"][
                visit_count[visit_key]
            ]

            expected_path = unit_test_data_for_visit["expected-complete-path"]
            assert fpath == expected_path, (
                f"Test Fail.\nAt {visit_key}, The actual path:\n "
                f"{pformat(fpath)}\n"
                f"did not match the expected path:\n{pformat(expected_path)}"
            )

            expected_summary = unit_test_data_for_visit[
                "expected-complete-path-summary"
            ]

            assert fpsum == expected_summary, (
                f"Test Fail.\nThe actual summary:\n"
                f"{fpsum} did not match the expected summary:\n"
                f"{expected_summary}"
            )

        visit_count[visit_key] = visit_count[visit_key] + 1

        return True

    # TODO:
    # Summarize Exit Path
    #   1. Exit Path Summary Function.
    #       (-> exit_path exit_path_contribution_ht)
    # Merge Exit Path Summaries
    #   2. Exit Summary Merge Function.
    #       (-> exit_path_contribution_ht_list final_contribution_ht)
    # Merge Basic Block Contribution
    #   3. Basic Block Summerize a Summary Function.
    #       (-> summarization_ht summary_ht summarization_ht)
    NS = NeckSearch(p, b, start_func, debug)

    # TODO: Pass in a "Full Path Summary Function" too.
    NS.visit_all(visit_func)

    print(f"TEST 4: END {msg}")


# #########################################################################

# TODO: Write test case with a branch that has a loop on one side and not on
# the other.


def main():
    # Legend:
    #   ^ is hop to loop exit
    #   * is call instruction to function
    #   N is neck
    #
    # If after first loop only, correct path is:
    # f0:b0,b1,b2,b3* -> f1:b0* -> f2:b0,b1^,b3N
    #
    # If after first (and all 3 getopt loops), correct path is:
    # f0:b0,b1,b2,b3* -> f1:b0,b1^,b3* -> f2:b0,b1^,b3N
    #
    b = {
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1", "b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b1": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b3"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b2": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b3"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b3": {  # Choke Point (nearest leader to non-main neck)
                    "inst": ["nop", "call f1", "call f1", "nop"],
                    "succ": ["b4"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b4": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b5", "b6"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b5": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b7"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b6": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b7"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b7": {  # Choke Point (farther away leader to non-main neck)
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b8"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b8": {  # NOT Choke Point (cause main())
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
            },
        },
        "f1": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "call f2", "nop"],  # not after loop
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b1": {  # Loop L0 start
                    "inst": ["nop", "nop", "call getopt"],  # GAF
                    "succ": ["b2"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b2": {  # Loop branch to L0 start
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1", "b3"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 1,
                    },
                },
                "b3": {  # Choke Point
                    "inst": ["nop", "call f2", "nop"],
                    "succ": ["b4"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 1,
                    },
                },
                "b4": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 1,
                    },
                },
            },
        },
        "f2": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b1": {  # Loop L0 start
                    "inst": ["nop", "nop", "call getopt"],  # GAF
                    "succ": ["b2"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b2": {  # Loop branch to L0 start
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1", "b3"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 1,
                    },
                },
                "b3": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b4"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 1,
                    },
                },
                "b4": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 1,
                    },
                },
            },
        },
        "f3": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "call f2", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
            },
        },
        "getopt": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
            },
        },
    }

    p = ["f0", "f1", "f2", "f3", "getopt"]

    # ###################################################################

    # In this graph:
    # No choke points
    # No function calls

    b0 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p0 = ["main"]

    # ###################################################################

    # In this graph:
    # Choke points
    # No function calls

    b1 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p1 = ["main"]

    # ###################################################################

    # In this graph:
    # Multiple Choke points
    # No function calls

    b1a = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b3"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b3": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p1a = ["main"]

    # ###################################################################

    # In this graph:
    # Multiple Choke points with loops in between
    # No function calls

    b1b = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {  # Loop head
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b3"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b3": {  # Work Inside Loop
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b4"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b4": {  # Loop branch decision
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b5", "b6"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        }
                    ],
                },
                "b5": {  # Jump to loop head.
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b2"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                        ("b5", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b6": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b7"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                        ("b6", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b7": {  # Loop head
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b8"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b8": {  # Work Inside Loop
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b9"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                        ("b8", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b9": {  # Loop branch decision
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b10", "b11"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                        ("b8", None, {}),
                                        ("b9", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b10": {  # Jump to loop head.
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b7"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                        ("b8", None, {}),
                                        ("b9", None, {}),
                                        ("b10", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b11": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b12"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                        ("b8", None, {}),
                                        ("b9", None, {}),
                                        ("b11", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b12": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                        ("b8", None, {}),
                                        ("b9", None, {}),
                                        ("b11", None, {}),
                                        ("b12", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p1b = ["main"]

    # ###################################################################

    # In this graph:
    # Choke points
    # Function calls with no recursion

    b2 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p2 = ["main", "f0"]

    # ###################################################################

    # In this graph:
    # Choke points
    # Function calls with no recursion, f0 has a loop
    # when we get to main@b2, the summarizing cp should mention the loop

    b2a = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                # Loop 1 ----------------------------------------
                "b1": {  # Loop start
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b2"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {  # Loop work
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b3"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b3": {  # Loop branch decision
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1", "b4"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                # Single Loop hidden in f0 -----------------------
                "b4": {  # Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": ["b5"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                # Loop 2 ----------------------------------------
                "b5": {  # Loop start
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b6"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        (
                                            "b4",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 1,
                                            },
                                        ),
                                        ("b5", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b6": {  # Loop work
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b7"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        (
                                            "b4",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 1,
                                            },
                                        ),
                                        ("b5", None, {}),
                                        ("b6", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 3,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b7": {  # Loop branch decision
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b5", "b8"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        (
                                            "b4",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 1,
                                            },
                                        ),
                                        ("b5", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 3,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                # Rest of main ----------------------------------------
                "b8": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b9"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        (
                                            "b4",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 1,
                                            },
                                        ),
                                        ("b5", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                        ("b8", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 3,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b9": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 2,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        (
                                            "b4",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 1,
                                            },
                                        ),
                                        ("b5", None, {}),
                                        ("b6", None, {}),
                                        ("b7", None, {}),
                                        ("b8", None, {}),
                                        ("b9", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 3,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", 1, {}),
                                    ],
                                ),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Loop start
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b2"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", 1, {}),
                                    ],
                                ),
                                ("f0", [("b0", None, {}), ("b1", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 1,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {  # Loop work
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b3"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", 1, {}),
                                    ],
                                ),
                                (
                                    "f0",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                    ],
                                ),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b3": {  # Loop branch decision
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1", "b4"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", 1, {}),
                                    ],
                                ),
                                (
                                    "f0",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                    ],
                                ),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b4": {  # Technical Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", 1, {}),
                                    ],
                                ),
                                (
                                    "f0",
                                    [
                                        ("b0", None, {}),
                                        ("b1", None, {}),
                                        ("b2", None, {}),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                    ],
                                ),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 2,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p2a = ["main", "f0"]

    # ###################################################################

    # In this graph:
    # Choke points
    # Function calls with no recursion, but they can be re-explored again.

    b3 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "call f0", "call f0", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {  # NOT Choke Point (cause main()): No recurse into f0
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            2,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                    ],
                                ),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p3 = ["main", "f0"]

    # ###################################################################

    # In this graph:
    # Choke points
    # Function calls with recursion, and they can be re-explored again.

    b3a = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "call f0", "call f0"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {  # NOT Choke Point (cause main()): No recurse into f0
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call f1", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            2,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                    ],
                                ),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f1": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", 1, {})]),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            2,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                    ],
                                ),
                                ("f0", [("b0", 1, {})]),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p3a = ["main", "f0", "f1"]

    # ###################################################################

    # In this graph:
    # Choke points
    # Function calls in not choke point but in participating function,
    # with no recursion, but they can be re-explored again.

    b3b = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {
                    "inst": ["nop", "call f1", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 1,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call f1", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f1": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", 1, {})]),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 1,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", 1, {}),
                                    ],
                                ),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
                "b1": {  # Inferred Choke Point
                    "inst": ["nop", "call f2", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", 1, {})]),
                                ("f1", [("b0", None, {}), ("b1", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 1,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", 1, {}),
                                    ],
                                ),
                                ("f1", [("b0", None, {}), ("b1", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
            },
        },
        "f2": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call getopt", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", 1, {})]),
                                ("f1", [("b0", None, {}), ("b1", 1, {})]),
                                ("f2", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 1,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", 1, {}),
                                    ],
                                ),
                                ("f1", [("b0", None, {}), ("b1", 1, {})]),
                                ("f2", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
                "b1": {  # Inferred Choke Point
                    "inst": ["nop", "call f1", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 1,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", 1, {})]),
                                ("f1", [("b0", None, {}), ("b1", 1, {})]),
                                ("f2", [("b0", None, {}), ("b1", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 1,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", 1, {}),
                                    ],
                                ),
                                ("f1", [("b0", None, {}), ("b1", 1, {})]),
                                ("f2", [("b0", None, {}), ("b1", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 2,
                            },
                        },
                    ],
                },
            },
        },
        "getopt": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # No unit test data because filtered out.
                },
            },
        },
    }

    p3b = ["main", "f0", "f1", "f2"]  # getopt is filtered out!

    # ###################################################################

    # In this graph:
    # Choke points
    # Function calls with simple recursion

    b4 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {  # NOT Choke Point (cause main())
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p4 = ["main", "f0"]

    # ###################################################################

    # In this graph:
    # Choke points
    # Function calls with farther back recursion
    # Ensure that getopt is only accounted for ONCE at any visit point.

    b5 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call getopt", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 1,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
                "b2": {
                    "inst": ["nop", "call f1", "nop"],
                    "succ": ["b3"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 1,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
                "b3": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b4"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 1,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        (
                                            "b2",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b3", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
                "b4": {
                    "inst": ["nop", "call f0", "nop", "call f1"],
                    "succ": ["b5"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 1,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        (
                                            "b2",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b3", None, {}),
                                        ("b4", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
                "b5": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 1,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        (
                                            "b2",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b3", None, {}),
                                        (
                                            "b4",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b5", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call f1", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", 1, {}),
                                    ],
                                ),
                                ("f1", [("b0", 1, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        (
                                            "b2",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b3", None, {}),
                                        ("b4", 1, {}),
                                    ],
                                ),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        (
                                            "b2",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b3", None, {}),
                                        (
                                            "b4",
                                            3,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                    ],
                                ),
                                ("f1", [("b0", 1, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
            },
        },
        "f1": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 1, {})]),
                                ("f0", [("b0", 1, {})]),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", 1, {}),
                                    ],
                                ),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        (
                                            "b2",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b3", None, {}),
                                        ("b4", 1, {}),
                                    ],
                                ),
                                ("f0", [("b0", 1, {})]),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        (
                                            "b2",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b3", None, {}),
                                        (
                                            "b4",
                                            3,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                    ],
                                ),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 1,
                            },
                        },
                    ],
                },
            },
        },
        "getopt": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # No unit test data because filtered out.
                },
            },
        },
    }

    p5 = ["main", "f0", "f1"]  # leave getopt out of the this set.

    # ###################################################################

    # In this graph:
    # Choke points
    # Function call with farther back recursion
    # Tests continuous backtracking loop after rejecting path recursion.

    b6 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [("main", [("b0", None, {})])],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "nop", "call f0"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", None, {})])
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
                "b2": {  # NOT Choke Point (cause main())
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                (
                                    "main",
                                    [
                                        ("b0", None, {}),
                                        (
                                            "b1",
                                            None,
                                            {
                                                "numSucceededGAF": 0,
                                                "numSucceededLoopHeads": 0,
                                            },
                                        ),
                                        ("b2", None, {}),
                                    ],
                                )
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "call f1"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 2, {})]),
                                ("f0", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f1": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "call f2"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 2, {})]),
                                ("f0", [("b0", 2, {})]),
                                ("f1", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
        "f2": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "call f0"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # Unit test validation data.
                    "unit-test-data": [
                        {
                            "expected-complete-path": [
                                ("main", [("b0", None, {}), ("b1", 2, {})]),
                                ("f0", [("b0", 2, {})]),
                                ("f1", [("b0", 2, {})]),
                                ("f2", [("b0", None, {})]),
                            ],
                            "expected-complete-path-summary": {
                                "numSucceededLoopHeads": 0,
                                "numSucceededGAF": 0,
                            },
                        },
                    ],
                },
            },
        },
    }

    p6 = ["main", "f0", "f1", "f2"]

    # ###################################################################

    # In this graph:
    # Choke points
    # Call getopt in a leaf

    # TODO: THIS GRAPH FAILS because the "call getopt" in the leaf is not
    # accounted for properly. Need to figure out what to do with it.
    # main@b1 should accumulate after its visiting the getopt call, and
    # main@b2 should see it when visited.

    b7 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "call f0", "nop"],
                    "succ": ["b2"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b2": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call getopt", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
            },
        },
        "getopt": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # No unit test data because filtered out.
                },
            },
        },
    }

    p7 = ["main", "f0"]  # getopt is filtered out.

    # ###################################################################

    # In this graph:
    # Choke points
    # Function calls with path duplication
    # Large Loop with bypass edge around it to choke point.
    #
    # TODO: THIS GRAPH IS KNOWN TO FAIL. It requires the "all paths" feature!
    #
    # The graph structure is designed so the BFS discovers the loop bypass edge
    # to the choke point before the observsation of the call to getopt inside
    # of the loop structure. The only way to fix this is if we had known
    # all the paths from entry to the choke point, plus their contributions,
    # and then aggregated them together into a single observation.

    b8 = {
        "main": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b1": {  # IF
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b5", "b2"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b2": {  # Loop head inside IF
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b3"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b3": {  # Loop work inside IF
                    "inst": ["nop", "call f0", "nop"],
                    "succ": ["b4"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b4": {  # Loop Branch to Loop head inside IF
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b5", "b2"],
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                },
                "b5": {  # Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": ["b6"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                },
                "b6": {
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": False,
                    "props": {
                        "numSucceededLoopHeads": 1,
                        "numSucceededGAF": 0,
                    },
                },
            },
        },
        "f0": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Inferred Choke Point
                    "inst": ["nop", "call getopt", "nop"],
                    "succ": ["b1"],
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                },
                "b1": {  # Choke Point
                    "inst": ["nop", "call getopt", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 1,
                    },
                },
            },
        },
        "getopt": {
            "entry": "b0",
            "bbs": {
                "b0": {  # Technical Choke Point
                    "inst": ["nop", "nop", "nop"],
                    "succ": None,
                    "chokep": True,
                    "props": {
                        "numSucceededLoopHeads": 0,
                        "numSucceededGAF": 0,
                    },
                    # No unit test data because filtered out.
                },
            },
        },
    }

    p8 = ["main", "f0"]  # getopt is fitered out

    # ###################################################################

    # OLD test3 stuff.
    # test3("b0", p, b, 'f0')
    # test3("b0", p0, b0, 'main')
    # test3("b1", p1, b1, 'main')
    # test3("b2", p2, b2, 'main')
    # test3("b3", p3, b3, 'main')
    # test3("b4", p4, b4, 'main')
    # test3("b5", p5, b5, 'main')
    # test3("b6", p6, b6, 'main')
    # test3("b7", p7, b7, 'main')

    # NEW Test4 stuff.
    # test4("b0", p, b, 'f0')

    test4("b0", p0, b0, "main", True)
    test4("b1", p1, b1, "main", True)
    test4("b1a", p1a, b1a, "main", True)
    test4("b1b", p1b, b1b, "main", True)

    test4("b2", p2, b2, "main", True)

    test4("b2a", p2a, b2a, "main", True)

    test4("b3", p3, b3, "main", True)
    test4("b3a", p3a, b3a, "main", True)
    test4("b3b", p3b, b3b, "main", True)

    test4("b4", p4, b4, "main", True)
    test4("b5", p5, b5, "main", True)
    test4("b6", p6, b6, "main", True)
    test4("b7", p7, b7, "main")
    test4("b8", p8, b8, "main")

    return 0


if __name__ == "__main__":
    os.sys.exit(main())
