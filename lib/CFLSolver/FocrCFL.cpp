//
// Created by kisslune on 30/5/23.
//

#include "CFLSolver/CFLSolver.h"

using namespace SVF;


void FocrCFL::initSolver()
{
    StdCFL::initSolver();
    /// Initialize ECG
    for (auto lbl : grammar()->transitiveSymbols)
        ecgs[lbl] = new ECG();
    /// Create ECG nodes
    for (auto it = graph()->begin(); it != graph()->end(); ++it)
    {
        for (auto lbl : grammar()->transitiveSymbols)
        {
            NodeID nId = it->first;
            ecgs[lbl]->addNode(nId);
        }
    }
    /// Remove transitive rules from binary-summarization list
    for (auto lbl : grammar()->transitiveSymbols)
        grammar()->binaryRules[std::make_pair(lbl, lbl)].erase(lbl);
}


void FocrCFL::processCFLItem(CFLItem item)
{
    /// Process primary transitive items
    if (grammar()->isTransitive(item.label().first) && isPrimary(item))
    {
        procPrimaryItem(item);
        return;
    }

    /// Process other items
    for (Label newTy : unarySumm(item.label()))
        if (checkAndAddEdge(item.src(), item.dst(), newTy))
            pushIntoWorklist(item.src(), item.dst(), newTy);

    for (auto& iter : cflData()->getSuccMap(item.dst()))
    {
        Label rty = iter.first;
        for (Label newTy : binarySumm(item.label(), rty))
            if (newTy == item.label() && grammar()->isTransitive(rty.first))
            {
                /// X ::= X A
                ECGNode* dst = ecgs[rty.first]->getNode(item.dst());
                checkSuccs(newTy, item.src(), dst);
            }
            else
            {
                NodeBS diffDsts = checkAndAddEdges(item.src(), iter.second, newTy);
                for (NodeID diffDst : diffDsts)
                    pushIntoWorklist(item.src(), diffDst, newTy);
            }
    }

    for (auto& iter : cflData()->getPredMap(item.src()))
    {
        Label lty = iter.first;
        for (Label newTy : binarySumm(lty, item.label()))
            if (newTy == item.label() && grammar()->isTransitive(lty.first))
            {
                /// X ::= A X
                ECGNode* src = ecgs[lty.first]->getNode(item.src());
                checkPreds(newTy, src, item.dst());
            }
            else
            {
                NodeBS diffSrcs = checkAndAddEdges(iter.second, item.dst(), newTy);
                for (NodeID diffSrc : diffSrcs)
                    pushIntoWorklist(diffSrc, item.dst(), newTy);
            }
    }
}


/*!
 * Transitive items generated in this methods are marked as secondary
 */
void FocrCFL::procPrimaryItem(CFLItem item)
{
    CFGSymbTy symb = item.label().first;
    NodeID src = item.src();
    NodeID dst = item.dst();

    if (ecgs[symb]->isReachable(src, dst))
        return;

    if (ecgs[symb]->isReachable(dst, src))    // src --> dst is a back edge
    {
        auto& newEdgeMap = ecgs[symb]->insertBackEdge(src, dst);
        for (auto& it : newEdgeMap)
        {
            cflData()->addEdges(it.first, it.second, item.label());
            for (auto newDst : it.second)
                pushIntoWorklist(it.first, newDst, item.label(), false);
        }
    }
    else
    {
        auto& newEdgeMap = ecgs[symb]->insertForwardEdge(src, dst);
        for (auto& it : newEdgeMap)
        {
            cflData()->addEdges(it.first, it.second, item.label());
            for (auto newDst : it.second)
                pushIntoWorklist(it.first, newDst, item.label(), false);
        }
    }
}


void FocrCFL::checkPreds(Label newLbl, ECGNode* src, NodeID dst)
{
    for (auto& pred : src->predecessors)
        if (checkAndAddEdge(pred->id, dst, newLbl))
        {
            pushIntoWorklist(pred->id, dst, newLbl);
            checkPreds(newLbl, pred, dst);
        }
}


void FocrCFL::checkSuccs(Label newLbl, NodeID src, ECGNode* dst)
{
    for (auto& succ : dst->successors)
        if (checkAndAddEdge(src, succ->id, newLbl))
        {
            pushIntoWorklist(src, succ->id, newLbl);
            checkSuccs(newLbl, src, succ);
        }
}


/*!
 * All the transitive items generated during ordinary solving is regarded as primary
 */
bool FocrCFL::pushIntoWorklist(NodeID src, NodeID dst, Label ty, bool isPrimary)
{
    return CFLBase::pushIntoWorklist(src, dst, ty, isPrimary);
}


void FocrCFL::countSumEdges()
{
    /// calculate checks
    for (auto it : ecgs)
        stat->checks += it.second->checks;

    StdCFL::countSumEdges();
}