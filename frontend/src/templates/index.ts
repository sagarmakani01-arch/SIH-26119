import type { SolverModel, SolverConstraint, SolverVariable } from "../services/api/types";

export type FieldType = "number" | "select" | "text";

export interface FieldDef {
  key: string;
  label: string;
  unit?: string;
  type: FieldType;
  min?: number;
  max?: number;
  step?: number;
  options?: { value: string; label: string }[];
  hint?: string;
}

export interface SectionDef {
  title: string;
  description?: string;
  fields: FieldDef[];
}

export interface BuildOutput {
  model: SolverModel;
  narrative: string[];
}

export interface TemplateDef {
  id: string;
  name: string;
  tag: "LP" | "MILP";
  description: string;
  syntheticNote: boolean;
  objectiveUnit: string;
  sections: SectionDef[];
  defaults: () => Record<string, number | string>;
  build: (inputs: Record<string, number | string>) => BuildOutput;
}

function num(inputs: Record<string, number | string>, key: string): number {
  const v = Number(inputs[key]);
  return Number.isFinite(v) ? v : 0;
}

function varC(count: number): SolverVariable[] {
  return Array.from({ length: count }, (_, i) => ({
    name: `x${i + 1}`,
    type: "continuous" as const,
    lb: 0,
    ub: null,
  }));
}

function cons(
  name: string,
  coefficients: number[],
  sense: SolverConstraint["sense"],
  rhs: number,
): SolverConstraint {
  return { name, sense, coefficients, rhs };
}

const refinery: TemplateDef = {
  id: "refinery",
  name: "Refinery Production Optimizer",
  tag: "LP",
  syntheticNote: true,
  description:
    "Blend three crude oils into petrol, diesel and jet fuel to maximize profit while meeting demand, capacity and quality targets.",
  objectiveUnit: "₹",
  sections: [
    {
      title: "Crude Oils",
      description: "Feedstock options — availability, cost and quality index (higher is better).",
      fields: [
        { key: "crudeA_avail", label: "Crude A availability", unit: "t", type: "number", min: 0 },
        { key: "crudeA_cost", label: "Crude A cost", unit: "₹/t", type: "number", min: 0 },
        { key: "crudeA_qual", label: "Crude A quality index", type: "number", min: 0, max: 100 },
        { key: "crudeB_avail", label: "Crude B availability", unit: "t", type: "number", min: 0 },
        { key: "crudeB_cost", label: "Crude B cost", unit: "₹/t", type: "number", min: 0 },
        { key: "crudeB_qual", label: "Crude B quality index", type: "number", min: 0, max: 100 },
        { key: "crudeC_avail", label: "Crude C availability", unit: "t", type: "number", min: 0 },
        { key: "crudeC_cost", label: "Crude C cost", unit: "₹/t", type: "number", min: 0 },
        { key: "crudeC_qual", label: "Crude C quality index", type: "number", min: 0, max: 100 },
      ],
    },
    {
      title: "Yields",
      description: "Fraction of each crude that becomes each product (0–1).",
      fields: [
        { key: "yA_petrol", label: "A → Petrol", type: "number", min: 0, max: 1, step: 0.01 },
        { key: "yA_diesel", label: "A → Diesel", type: "number", min: 0, max: 1, step: 0.01 },
        { key: "yA_jet", label: "A → Jet fuel", type: "number", min: 0, max: 1, step: 0.01 },
        { key: "yB_petrol", label: "B → Petrol", type: "number", min: 0, max: 1, step: 0.01 },
        { key: "yB_diesel", label: "B → Diesel", type: "number", min: 0, max: 1, step: 0.01 },
        { key: "yB_jet", label: "B → Jet fuel", type: "number", min: 0, max: 1, step: 0.01 },
        { key: "yC_petrol", label: "C → Petrol", type: "number", min: 0, max: 1, step: 0.01 },
        { key: "yC_diesel", label: "C → Diesel", type: "number", min: 0, max: 1, step: 0.01 },
        { key: "yC_jet", label: "C → Jet fuel", type: "number", min: 0, max: 1, step: 0.01 },
      ],
    },
    {
      title: "Market",
      description: "Product prices, demand targets and plant limits.",
      fields: [
        { key: "price_petrol", label: "Petrol price", unit: "₹/t", type: "number", min: 0 },
        { key: "price_diesel", label: "Diesel price", unit: "₹/t", type: "number", min: 0 },
        { key: "price_jet", label: "Jet fuel price", unit: "₹/t", type: "number", min: 0 },
        { key: "demand_petrol", label: "Petrol demand", unit: "t", type: "number", min: 0 },
        { key: "demand_diesel", label: "Diesel demand", unit: "t", type: "number", min: 0 },
        { key: "demand_jet", label: "Jet fuel demand", unit: "t", type: "number", min: 0 },
        { key: "capacity", label: "Processing capacity", unit: "t", type: "number", min: 0 },
        { key: "min_quality", label: "Minimum blend quality", type: "number", min: 0, max: 100 },
      ],
    },
  ],
  defaults: () => ({
    crudeA_avail: 10000,
    crudeA_cost: 450,
    crudeA_qual: 82,
    crudeB_avail: 8000,
    crudeB_cost: 520,
    crudeB_qual: 90,
    crudeC_avail: 6000,
    crudeC_cost: 400,
    crudeC_qual: 74,
    yA_petrol: 0.45,
    yA_diesel: 0.35,
    yA_jet: 0.15,
    yB_petrol: 0.55,
    yB_diesel: 0.3,
    yB_jet: 0.1,
    yC_petrol: 0.35,
    yC_diesel: 0.45,
    yC_jet: 0.12,
    price_petrol: 700,
    price_diesel: 650,
    price_jet: 720,
    demand_petrol: 5000,
    demand_diesel: 4000,
    demand_jet: 1500,
    capacity: 15000,
    min_quality: 80,
  }),
  build: (inputs) => {
    const n = num;
    const crudeNames = ["crudeA", "crudeB", "crudeC"];
    const crudeLetters = ["A", "B", "C"];
    const productKeys = ["petrol", "diesel", "jet"];
    const crudes = crudeNames.map((c) => ({
      avail: n(inputs, `${c}_avail`),
      cost: n(inputs, `${c}_cost`),
      qual: n(inputs, `${c}_qual`),
    }));
    const products = productKeys.map((p) => ({
      price: n(inputs, `price_${p}`),
      demand: n(inputs, `demand_${p}`),
      yields: crudeLetters.map((letter) => n(inputs, `y${letter}_${p}`)),
    }));
    const capacity = n(inputs, "capacity");
    const minQuality = n(inputs, "min_quality");

    const variables: SolverVariable[] = [
      ...crudes.map((c, i) => ({
        name: `Crude ${String.fromCharCode(65 + i)}`,
        type: "continuous" as const,
        lb: 0,
        ub: c.avail,
      })),
      ...productKeys.map((p) => ({
        name: p[0].toUpperCase() + p.slice(1),
        type: "continuous" as const,
        lb: 0,
        ub: null,
      })),
    ];

    const constraints: SolverConstraint[] = [];
    constraints.push(
      cons(
        "Processing capacity",
        [1, 1, 1, 0, 0, 0],
        "<=",
        capacity,
      ),
    );
    productKeys.forEach((p, j) => {
      const coeff = [...crudes.map((_, i) => -products[j].yields[i]), 0, 0, 0];
      coeff[3 + j] = 1;
      constraints.push(cons(`Balance — ${p}`, coeff, "=", 0));
    });
    productKeys.forEach((p, j) => {
      const coeff = [0, 0, 0, 0, 0, 0];
      coeff[3 + j] = 1;
      constraints.push(cons(`Demand — ${p}`, coeff, ">=", products[j].demand));
    });
    constraints.push(
      cons(
        "Blend quality",
        [...crudes.map((c) => c.qual - minQuality), 0, 0, 0],
        ">=",
        0,
      ),
    );

    const objectiveCoeffs = [
      -crudes[0].cost,
      -crudes[1].cost,
      -crudes[2].cost,
      products[0].price,
      products[1].price,
      products[2].price,
    ];

    const model: SolverModel = {
      name: "Refinery Production Optimizer",
      objective: { sense: "maximize", coefficients: objectiveCoeffs },
      variables,
      constraints,
      config: { presolve: true, verify: true, backend: "auto" },
    };

    const narrative = [
      `Maximize profit = product revenue (petrol, diesel, jet fuel at market price) minus crude purchase cost.`,
      `6 decision variables: purchase/usage of 3 crudes (bounded by availability) and production of 3 products (>= 0).`,
      `8 constraints: total throughput <= ${capacity} t processing capacity; product balances link output to crude mix via yields; product output >= demand (${products[0].demand} / ${products[1].demand} / ${products[2].demand} t); blended quality index >= ${minQuality}.`,
      `All data is synthetic demo data (not MRPL plant data).`,
    ];

    return { model, narrative };
  },
};

const production: TemplateDef = {
  id: "production",
  name: "Production Planning",
  tag: "LP",
  syntheticNote: true,
  description:
    "Allocate three shared resources across three products to maximize contribution while respecting capacity and demand caps.",
  objectiveUnit: "₹",
  sections: [
    {
      title: "Resources",
      description: "Total available capacity of each shared resource.",
      fields: [
        { key: "r1_avail", label: "Resource 1 availability", unit: "units", type: "number", min: 0 },
        { key: "r2_avail", label: "Resource 2 availability", unit: "units", type: "number", min: 0 },
        { key: "r3_avail", label: "Resource 3 availability", unit: "units", type: "number", min: 0 },
      ],
    },
    {
      title: "Products",
      description: "Contribution margin, market demand cap and resource usage per unit.",
      fields: [
        { key: "p1_profit", label: "Product 1 contribution", unit: "₹/unit", type: "number" },
        { key: "p2_profit", label: "Product 2 contribution", unit: "₹/unit", type: "number" },
        { key: "p3_profit", label: "Product 3 contribution", unit: "₹/unit", type: "number" },
        { key: "p1_demand", label: "Product 1 demand cap", unit: "units", type: "number", min: 0 },
        { key: "p2_demand", label: "Product 2 demand cap", unit: "units", type: "number", min: 0 },
        { key: "p3_demand", label: "Product 3 demand cap", unit: "units", type: "number", min: 0 },
        { key: "a11", label: "P1 × R1 usage", type: "number", min: 0, step: 0.1 },
        { key: "a12", label: "P2 × R1 usage", type: "number", min: 0, step: 0.1 },
        { key: "a13", label: "P3 × R1 usage", type: "number", min: 0, step: 0.1 },
        { key: "a21", label: "P1 × R2 usage", type: "number", min: 0, step: 0.1 },
        { key: "a22", label: "P2 × R2 usage", type: "number", min: 0, step: 0.1 },
        { key: "a23", label: "P3 × R2 usage", type: "number", min: 0, step: 0.1 },
        { key: "a31", label: "P1 × R3 usage", type: "number", min: 0, step: 0.1 },
        { key: "a32", label: "P2 × R3 usage", type: "number", min: 0, step: 0.1 },
        { key: "a33", label: "P3 × R3 usage", type: "number", min: 0, step: 0.1 },
      ],
    },
  ],
  defaults: () => ({
    r1_avail: 2400,
    r2_avail: 1800,
    r3_avail: 900,
    p1_profit: 40,
    p2_profit: 30,
    p3_profit: 50,
    p1_demand: 100,
    p2_demand: 150,
    p3_demand: 80,
    a11: 2,
    a12: 1,
    a13: 2,
    a21: 1,
    a22: 2,
    a23: 1,
    a31: 1,
    a32: 1,
    a33: 2,
  }),
  build: (inputs) => {
    const n = num;
    const avail = [n(inputs, "r1_avail"), n(inputs, "r2_avail"), n(inputs, "r3_avail")];
    const profit = [n(inputs, "p1_profit"), n(inputs, "p2_profit"), n(inputs, "p3_profit")];
    const demand = [n(inputs, "p1_demand"), n(inputs, "p2_demand"), n(inputs, "p3_demand")];
    const a = [
      [n(inputs, "a11"), n(inputs, "a12"), n(inputs, "a13")],
      [n(inputs, "a21"), n(inputs, "a22"), n(inputs, "a23")],
      [n(inputs, "a31"), n(inputs, "a32"), n(inputs, "a33")],
    ];

    const variables: SolverVariable[] = [1, 2, 3].map((i) => ({
      name: `Product ${i}`,
      type: "continuous",
      lb: 0,
      ub: demand[i - 1],
    }));

    const constraints: SolverConstraint[] = avail.map((limit, i) =>
      cons(`Resource ${i + 1} capacity`, a[i], "<=", limit),
    );

    const model: SolverModel = {
      name: "Production Planning",
      objective: { sense: "maximize", coefficients: profit },
      variables,
      constraints,
      config: { presolve: true, verify: true, backend: "auto" },
    };

    const narrative = [
      `Maximize contribution margin across 3 products.`,
      `3 continuous production quantities, each capped by its market demand (${demand[0]} / ${demand[1]} / ${demand[2]} units).`,
      `3 capacity constraints: weighted resource usage must stay within availability (${avail[0]} / ${avail[1]} / ${avail[2]} units).`,
      `All data is synthetic demo data.`,
    ];

    return { model, narrative };
  },
};

const logistics: TemplateDef = {
  id: "logistics",
  name: "Logistics Network",
  tag: "LP",
  syntheticNote: true,
  description:
    "Ship goods from two depots to three delivery zones at minimum total freight cost while meeting every zone's demand.",
  objectiveUnit: "₹",
  sections: [
    {
      title: "Depots & Zones",
      description: "Supply at each depot, demand at each delivery zone, and per-route freight cost.",
      fields: [
        { key: "s1", label: "Depot 1 supply", unit: "t", type: "number", min: 0 },
        { key: "s2", label: "Depot 2 supply", unit: "t", type: "number", min: 0 },
        { key: "d1", label: "Zone 1 demand", unit: "t", type: "number", min: 0 },
        { key: "d2", label: "Zone 2 demand", unit: "t", type: "number", min: 0 },
        { key: "d3", label: "Zone 3 demand", unit: "t", type: "number", min: 0 },
        { key: "c11", label: "Depot 1 → Zone 1 cost", unit: "₹/t", type: "number", min: 0 },
        { key: "c12", label: "Depot 1 → Zone 2 cost", unit: "₹/t", type: "number", min: 0 },
        { key: "c13", label: "Depot 1 → Zone 3 cost", unit: "₹/t", type: "number", min: 0 },
        { key: "c21", label: "Depot 2 → Zone 1 cost", unit: "₹/t", type: "number", min: 0 },
        { key: "c22", label: "Depot 2 → Zone 2 cost", unit: "₹/t", type: "number", min: 0 },
        { key: "c23", label: "Depot 2 → Zone 3 cost", unit: "₹/t", type: "number", min: 0 },
      ],
    },
  ],
  defaults: () => ({
    s1: 800,
    s2: 700,
    d1: 300,
    d2: 400,
    d3: 500,
    c11: 40,
    c12: 60,
    c13: 55,
    c21: 45,
    c22: 35,
    c23: 70,
  }),
  build: (inputs) => {
    const n = num;
    const supply = [n(inputs, "s1"), n(inputs, "s2")];
    const demand = [n(inputs, "d1"), n(inputs, "d2"), n(inputs, "d3")];
    const cost = [
      [n(inputs, "c11"), n(inputs, "c12"), n(inputs, "c13")],
      [n(inputs, "c21"), n(inputs, "c22"), n(inputs, "c23")],
    ];

    const variables: SolverVariable[] = [];
    for (let i = 0; i < 2; i++) {
      for (let j = 0; j < 3; j++) {
        variables.push({
          name: `D${i + 1}→Z${j + 1}`,
          type: "continuous",
          lb: 0,
          ub: supply[i],
        });
      }
    }

    const constraints: SolverConstraint[] = [];
    for (let i = 0; i < 2; i++) {
      const coefficients = [0, 0, 0, 0, 0, 0];
      coefficients[i * 3] = 1;
      coefficients[i * 3 + 1] = 1;
      coefficients[i * 3 + 2] = 1;
      constraints.push(cons(`Depot ${i + 1} supply`, coefficients, "<=", supply[i]));
    }
    for (let j = 0; j < 3; j++) {
      const coefficients = [0, 0, 0, 0, 0, 0];
      coefficients[j] = 1;
      coefficients[3 + j] = 1;
      constraints.push(cons(`Zone ${j + 1} demand`, coefficients, ">=", demand[j]));
    }

    const objectiveCoeffs = [cost[0][0], cost[0][1], cost[0][2], cost[1][0], cost[1][1], cost[1][2]];

    const model: SolverModel = {
      name: "Logistics Network",
      objective: { sense: "minimize", coefficients: objectiveCoeffs },
      variables,
      constraints,
      config: { presolve: true, verify: true, backend: "auto" },
    };

    const narrative = [
      `Minimize total freight cost across 2 depots × 3 delivery zones (6 shipment routes).`,
      `Depot outflow <= available supply (${supply[0]} / ${supply[1]} t); zone inflow >= demand (${demand[0]} / ${demand[1]} / ${demand[2]} t).`,
      `All flows are non-negative continuous quantities.`,
      `All data is synthetic demo data.`,
    ];

    return { model, narrative };
  },
};

const resource: TemplateDef = {
  id: "resource",
  name: "Capital & Effort Allocation",
  tag: "LP",
  syntheticNote: true,
  description:
    "Select how many units of each initiative to fund within a fixed budget and staffing envelope.",
  objectiveUnit: "₹",
  sections: [
    {
      title: "Envelope",
      fields: [
        { key: "budget", label: "Total budget", unit: "₹", type: "number", min: 0 },
        { key: "hours", label: "Total effort", unit: "person-hours", type: "number", min: 0 },
      ],
    },
    {
      title: "Initiatives",
      description: "Value delivered, budget and effort consumed per unit, and maximum sensible scale.",
      fields: [
        { key: "v1", label: "Initiative 1 value", unit: "₹/unit", type: "number" },
        { key: "v2", label: "Initiative 2 value", unit: "₹/unit", type: "number" },
        { key: "v3", label: "Initiative 3 value", unit: "₹/unit", type: "number" },
        { key: "b1", label: "Initiative 1 budget / unit", unit: "₹", type: "number", min: 0 },
        { key: "b2", label: "Initiative 2 budget / unit", unit: "₹", type: "number", min: 0 },
        { key: "b3", label: "Initiative 3 budget / unit", unit: "₹", type: "number", min: 0 },
        { key: "h1", label: "Initiative 1 effort / unit", unit: "h", type: "number", min: 0 },
        { key: "h2", label: "Initiative 2 effort / unit", unit: "h", type: "number", min: 0 },
        { key: "h3", label: "Initiative 3 effort / unit", unit: "h", type: "number", min: 0 },
        { key: "m1", label: "Initiative 1 max units", type: "number", min: 0 },
        { key: "m2", label: "Initiative 2 max units", type: "number", min: 0 },
        { key: "m3", label: "Initiative 3 max units", type: "number", min: 0 },
      ],
    },
  ],
  defaults: () => ({
    budget: 100000,
    hours: 600,
    v1: 25,
    v2: 40,
    v3: 35,
    b1: 2000,
    b2: 3500,
    b3: 2500,
    h1: 4,
    h2: 6,
    h3: 5,
    m1: 40,
    m2: 30,
    m3: 35,
  }),
  build: (inputs) => {
    const n = num;
    const budget = n(inputs, "budget");
    const hours = n(inputs, "hours");
    const value = [n(inputs, "v1"), n(inputs, "v2"), n(inputs, "v3")];
    const bCost = [n(inputs, "b1"), n(inputs, "b2"), n(inputs, "b3")];
    const hCost = [n(inputs, "h1"), n(inputs, "h2"), n(inputs, "h3")];
    const maxUnits = [n(inputs, "m1"), n(inputs, "m2"), n(inputs, "m3")];

    const variables: SolverVariable[] = [1, 2, 3].map((i) => ({
      name: `Initiative ${i}`,
      type: "continuous",
      lb: 0,
      ub: maxUnits[i - 1],
    }));

    const constraints: SolverConstraint[] = [
      cons("Budget envelope", bCost, "<=", budget),
      cons("Effort envelope", hCost, "<=", hours),
    ];

    const model: SolverModel = {
      name: "Capital & Effort Allocation",
      objective: { sense: "maximize", coefficients: value },
      variables,
      constraints,
      config: { presolve: true, verify: true, backend: "auto" },
    };

    const narrative = [
      `Maximize delivered value across 3 initiatives.`,
      `2 constraints: total spend <= ₹${budget.toLocaleString("en-IN")} and total effort <= ${hours} person-hours.`,
      `Each initiative is bounded by its maximum sensible scale (${maxUnits[0]} / ${maxUnits[1]} / ${maxUnits[2]} units).`,
      `All data is synthetic demo data.`,
    ];

    return { model, narrative };
  },
};

const scheduling: TemplateDef = {
  id: "scheduling",
  name: "Shift Assignment (MILP)",
  tag: "MILP",
  syntheticNote: true,
  description:
    "Assign exactly one worker to each of three tasks at minimum cost — a binary assignment problem solved by branch & bound.",
  objectiveUnit: "₹",
  sections: [
    {
      title: "Assignment Costs",
      description: "Cost of assigning each worker to each task (₹).",
      fields: [
        { key: "c11", label: "Worker 1 → Task 1", unit: "₹", type: "number", min: 0 },
        { key: "c12", label: "Worker 1 → Task 2", unit: "₹", type: "number", min: 0 },
        { key: "c13", label: "Worker 1 → Task 3", unit: "₹", type: "number", min: 0 },
        { key: "c21", label: "Worker 2 → Task 1", unit: "₹", type: "number", min: 0 },
        { key: "c22", label: "Worker 2 → Task 2", unit: "₹", type: "number", min: 0 },
        { key: "c23", label: "Worker 2 → Task 3", unit: "₹", type: "number", min: 0 },
        { key: "c31", label: "Worker 3 → Task 1", unit: "₹", type: "number", min: 0 },
        { key: "c32", label: "Worker 3 → Task 2", unit: "₹", type: "number", min: 0 },
        { key: "c33", label: "Worker 3 → Task 3", unit: "₹", type: "number", min: 0 },
      ],
    },
  ],
  defaults: () => ({
    c11: 40,
    c12: 60,
    c13: 55,
    c21: 45,
    c22: 35,
    c23: 70,
    c31: 50,
    c32: 55,
    c33: 45,
  }),
  build: (inputs) => {
    const n = num;
    const cost = [
      [n(inputs, "c11"), n(inputs, "c12"), n(inputs, "c13")],
      [n(inputs, "c21"), n(inputs, "c22"), n(inputs, "c23")],
      [n(inputs, "c31"), n(inputs, "c32"), n(inputs, "c33")],
    ];

    const variables: SolverVariable[] = [];
    for (let i = 0; i < 3; i++) {
      for (let j = 0; j < 3; j++) {
        variables.push({
          name: `W${i + 1}→T${j + 1}`,
          type: "binary",
          lb: 0,
          ub: 1,
        });
      }
    }

    const constraints: SolverConstraint[] = [];
    for (let j = 0; j < 3; j++) {
      const coefficients = new Array(9).fill(0);
      for (let i = 0; i < 3; i++) coefficients[i * 3 + j] = 1;
      constraints.push(cons(`Task ${j + 1} covered`, coefficients, "=", 1));
    }
    for (let i = 0; i < 3; i++) {
      const coefficients = new Array(9).fill(0);
      coefficients[i * 3] = 1;
      coefficients[i * 3 + 1] = 1;
      coefficients[i * 3 + 2] = 1;
      constraints.push(cons(`Worker ${i + 1} capacity`, coefficients, "<=", 1));
    }

    const objectiveCoeffs: number[] = [];
    for (let i = 0; i < 3; i++) {
      for (let j = 0; j < 3; j++) objectiveCoeffs.push(cost[i][j]);
    }

    const model: SolverModel = {
      name: "Shift Assignment (MILP)",
      objective: { sense: "minimize", coefficients: objectiveCoeffs },
      variables,
      constraints,
      config: { presolve: true, verify: true, backend: "auto", mip_rel_gap: 0 },
    };

    const narrative = [
      `Minimize total assignment cost: 9 binary variables (worker i assigned to task j or not).`,
      `Each task must be covered by exactly one worker; each worker can take at most one task.`,
      `Solved as MILP: LP relaxation + branch & bound over the 9 binary decisions.`,
      `All data is synthetic demo data.`,
    ];

    return { model, narrative };
  },
};

export const templates: TemplateDef[] = [refinery, production, logistics, resource, scheduling];

export const templateMap: Record<string, TemplateDef> = Object.fromEntries(
  templates.map((t) => [t.id, t]),
);

export function getTemplate(id: string | null | undefined): TemplateDef | null {
  if (!id) return null;
  return templateMap[id] ?? null;
}

export { varC };
