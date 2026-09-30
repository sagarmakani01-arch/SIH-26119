import {
  Bar,
  BarChart,
  CartesianGrid,
  Cell,
  Legend,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";
import type { SolveResult, SolverModel } from "../services/api/types";

const AXIS = "#64769a";
const GRID = "#1c2942";

const tooltipStyle = {
  background: "#0c1322",
  border: "1px solid #27395c",
  borderRadius: "10px",
  fontSize: 12,
  color: "#e6edf7",
};

export function AllocationChart({ model, result }: { model: SolverModel; result: SolveResult }) {
  const data = model.variables.map((v, i) => ({
    name: v.name,
    value: Number((result.x[i] ?? 0).toFixed(4)),
  }));
  return (
    <div className="h-64 w-full">
      <ResponsiveContainer width="100%" height="100%">
        <BarChart data={data} margin={{ top: 8, right: 12, left: -8, bottom: 4 }}>
          <CartesianGrid stroke={GRID} strokeDasharray="3 3" vertical={false} />
          <XAxis dataKey="name" tick={{ fill: AXIS, fontSize: 11 }} tickLine={false} axisLine={false} />
          <YAxis tick={{ fill: AXIS, fontSize: 11 }} tickLine={false} axisLine={false} />
          <Tooltip contentStyle={tooltipStyle} cursor={{ fill: "rgba(34,211,238,0.06)" }} />
          <Bar dataKey="value" name="Value" radius={[6, 6, 0, 0]}>
            {data.map((_, i) => (
              <Cell key={i} fill={i % 2 === 0 ? "#22d3ee" : "#8b5cf6"} />
            ))}
          </Bar>
        </BarChart>
      </ResponsiveContainer>
    </div>
  );
}

export function UtilizationChart({
  model,
  result,
}: {
  model: SolverModel;
  result: SolveResult;
}) {
  const data = model.constraints.map((c, i) => {
    const activity = result.constraint_activities[i] ?? 0;
    const rhs = c.rhs;
    const capacity = Math.abs(rhs) > 1e-9 ? Math.abs(rhs) : Math.abs(activity) || 1;
    const utilization = Math.min(2, Math.abs(activity) / capacity);
    return {
      name: c.name ?? `c${i + 1}`,
      utilization: Number((utilization * 100).toFixed(1)),
    };
  });
  return (
    <div className="h-64 w-full">
      <ResponsiveContainer width="100%" height="100%">
        <BarChart data={data} layout="vertical" margin={{ top: 8, right: 16, left: 24, bottom: 4 }}>
          <CartesianGrid stroke={GRID} strokeDasharray="3 3" horizontal={false} />
          <XAxis
            type="number"
            unit="%"
            tick={{ fill: AXIS, fontSize: 11 }}
            tickLine={false}
            axisLine={false}
          />
          <YAxis
            type="category"
            dataKey="name"
            width={120}
            tick={{ fill: AXIS, fontSize: 10 }}
            tickLine={false}
            axisLine={false}
          />
          <Tooltip contentStyle={tooltipStyle} cursor={{ fill: "rgba(34,211,238,0.06)" }} />
          <ReferenceLine x={100} stroke="#fbbf24" strokeDasharray="4 4" />
          <Bar dataKey="utilization" name="Utilization" radius={[0, 6, 6, 0]}>
            {data.map((d, i) => (
              <Cell
                key={i}
                fill={
                  d.utilization > 99.5 ? "#34d399" : d.utilization > 80 ? "#22d3ee" : "#3b4d75"
                }
              />
            ))}
          </Bar>
        </BarChart>
      </ResponsiveContainer>
    </div>
  );
}

export interface ScenarioPoint {
  name: string;
  objective: number;
}

export function ScenarioChart({ data, sense }: { data: ScenarioPoint[]; sense: string }) {
  return (
    <div className="h-72 w-full">
      <ResponsiveContainer width="100%" height="100%">
        <BarChart data={data} margin={{ top: 8, right: 12, left: -4, bottom: 4 }}>
          <CartesianGrid stroke={GRID} strokeDasharray="3 3" vertical={false} />
          <XAxis dataKey="name" tick={{ fill: AXIS, fontSize: 11 }} tickLine={false} axisLine={false} />
          <YAxis tick={{ fill: AXIS, fontSize: 11 }} tickLine={false} axisLine={false} />
          <Tooltip contentStyle={tooltipStyle} cursor={{ fill: "rgba(34,211,238,0.06)" }} />
          <Legend wrapperStyle={{ fontSize: 11, color: AXIS }} />
          <Bar dataKey="objective" name={`Objective (${sense})`} radius={[6, 6, 0, 0]}>
            {data.map((_, i) => (
              <Cell key={i} fill={i === 0 ? "#22d3ee" : "#8b5cf6"} />
            ))}
          </Bar>
        </BarChart>
      </ResponsiveContainer>
    </div>
  );
}
