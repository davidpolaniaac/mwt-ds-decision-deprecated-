﻿using System;
using System.Collections.Generic;
using System.IO;

namespace Microsoft.Research.MultiWorldTesting.ExploreLibrary
{
    /// <summary>
    /// Represents a recorder that exposes a method to record exploration data based on generic contexts. 
    /// </summary>
    /// <typeparam name="TContext">The Context type.</typeparam>
    /// <remarks>
    /// Exploration data is specified as a set of tuples (context, action, probability, key) as described below. An 
    /// application passes an IRecorder object to the @MwtExplorer constructor. See 
    /// @StringRecorder for a sample IRecorder object.
    /// </remarks>
    public interface IRecorder<in TContext, in TValue>
    {
        /// <summary>
        /// Records the exploration data associated with a given decision.
        /// This implementation should be thread-safe if multithreading is needed.
        /// </summary>
        /// <param name="context">A user-defined context for the decision.</param>
        /// <param name="action">Chosen by an exploration algorithm given context.</param>
        /// <param name="probability">The probability of the chosen action given context.</param>
        /// <param name="uniqueKey">A user-defined identifer for the decision.</param>
        /// <param name="modelId">Optional; The Id of the model used to make predictions/decisions, if any exists at decision time.</param>
        /// <param name="isExplore">Optional; Indicates whether the decision was generated purely from exploration (vs. exploitation).</param>
        void Record(TContext context, TValue value, object explorerState, object mapperState, UniqueEventID uniqueKey); 
    }

    public interface IExplorer<TValue, TMapperValue>
    {
        /// <summary>
        /// Determines the action to take and the probability with which it was chosen, for a
        /// given context. 
        /// </summary>
        /// <param name="saltedSeed">A PRG seed based on a unique id information provided by the user.</param>
        /// <param name="context">A user-defined context for the decision.</param>
        /// <param name="numActionsVariable">Optional; Number of actions available which may be variable across decisions.</param>
        /// <returns>
        /// A <see cref="DecisionTuple"/> object including the action to take, the probability it was chosen, 
        /// and a flag indicating whether to record this decision.
        /// </returns>
        ExplorerDecision<TValue> MapContext(ulong saltedSeed, TMapperValue policyAction); 

        void EnableExplore(bool explore);
    }

    public interface IFullExplorer<in TContext, TValue>
    {
        ExplorerDecision<TValue> Explore(ulong saltedSeed, TContext context); 
    }

    public interface IVariableActionExplorer<TValue, in TMapperValue>
    {
        // TODO: review xml docs
        ExplorerDecision<TValue> Explore(ulong saltedSeed, TMapperValue policyAction, int numActions);
    }
    
    public interface INumberOfActionsProvider<in TContext>
    {
        int GetNumberOfActions(TContext context);
    }

    public interface IContextMapper<in TContext, TValue>
    {
        /// <summary>
        /// Determines the action to take for a given context.
        /// This implementation should be thread-safe if multithreading is needed.
        /// </summary>
        /// <param name="context">A user-defined context for the decision.</param>
        /// <param name="numActionsVariable">Optional; Number of actions available which may be variable across decisions.</param>
        /// <returns>A decision tuple containing the index of the action to take (1-based), and the Id of the model or policy used to make the decision.
        /// Can be null if the Policy is not ready yet (e.g. model not loaded).</returns>
        PolicyDecision<TValue> MapContext(TContext context);
    }

    public interface IUpdatable<TModel>
    {
        void Update(TModel model);
    }

    public interface IPolicy<in TContext> : IContextMapper<TContext, int>
    {
    }

    public interface IRanker<in TContext> : IContextMapper<TContext, int[]>
    {
    }

    public interface IScorer<in TContext> : IContextMapper<TContext, float[]>
    {
    }
}