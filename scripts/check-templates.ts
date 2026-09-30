import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { writeFileSync } from "node:fs";
import { templates } from "../frontend/src/templates";

const cli = join(process.cwd(), "build", "solver", "solver_cli.exe");
let failures = 0;

for (const t of templates) {
  const { model } = t.build(t.defaults());
  const file = join(tmpdir(), `t_${t.id}.json`);
  writeFileSync(file, JSON.stringify(model), "utf8");
  const res = spawnSync(cli, [], { input: JSON.stringify(model), encoding: "utf8" });
  const out = JSON.parse(res.stdout);
  const ok = ["OPTIMAL", "FEASIBLE"].includes(out.status);
  if (!ok) failures++;
  console.log(
    `${ok ? "OK  " : "FAIL"} ${t.id.padEnd(12)} ${out.status.padEnd(10)} obj=${out.objective} vars=${model.variables.length} cons=${model.constraints.length} ${out.termination_reason ?? ""}`,
  );
}

const big = spawnSync(cli, ["--validate"], {
  input: JSON.stringify(templates[0].build(templates[0].defaults()).model),
  encoding: "utf8",
});
console.log("validate-refinery:", big.stdout.trim());

process.exit(failures ? 1 : 0);
