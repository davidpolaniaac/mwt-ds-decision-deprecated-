using MultiWorldTesting;
using System;
using System.Collections.Generic;
using System.Linq;

namespace TestCommon
{
    public class TestPolicy<TContext> : IPolicy<TContext>
    {
        public TestPolicy(uint numberOfActions) : this(numberOfActions, -1) { }

        public TestPolicy(uint numberOfActions, int index)
        {
            this.numberOfActions = numberOfActions;
            this.index = index;
            this.ActionToChoose = uint.MaxValue;
        }

        public uint[] ChooseAction(TContext context)
        {
            uint[] actions = new uint[numberOfActions];
            for (int i = 0; i < actions.Length; i++)
            {
                actions[i] = (uint)(i + 1);
            }

            uint topActionToChoose = (this.ActionToChoose != uint.MaxValue) ? this.ActionToChoose : 5;
            
            // swap action to choose with top one
            uint topAction = actions[0];
            actions[0] = actions[topActionToChoose - 1];
            actions[topActionToChoose - 1] = topAction;

            return actions;
        }

        public uint ActionToChoose { get; set; }
        private int index;
        private uint numberOfActions;
    }

    public class TestScorer<Ctx> : IScorer<Ctx>
    {
        public TestScorer(int param, uint numActions, bool uniform = true)
        {
            this.param = param;
            this.uniform = uniform;
            this.numActions = numActions;
        }
        public List<float> ScoreActions(Ctx context)
        {
            if (uniform)
            {
                return Enumerable.Repeat<float>(param, (int)numActions).ToList();
            }
            else
            {
                return Array.ConvertAll<int, float>(Enumerable.Range(param, (int)numActions).ToArray(), Convert.ToSingle).ToList();
            }
        }
        private int param;
        private uint numActions;
        private bool uniform;
    }

    public class RegularTestContext : IStringContext
    {
        private int id;

        public int Id
        {
            get { return id; }
            set { id = value; }
        }

        public override string ToString()
        {
            return id.ToString();
        }
    }

    public class VariableActionTestContext : RegularTestContext, IVariableActionContext
    {
        public VariableActionTestContext(uint numberOfActions)
        {
            NumberOfActions = numberOfActions;
        }

        public uint GetNumberOfActions()
        {
            return NumberOfActions;
        }

        public uint NumberOfActions { get; set; }
    }
}
