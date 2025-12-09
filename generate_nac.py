import yaml
from jinja2 import Environment, FileSystemLoader
import os

INPUT_TOPOLOGY = 'input/topology.yaml'
INPUT_POLICY = 'input/nac-policy.yaml'
OUTPUT_FILE = 'output/ndn-nac.yaml'
TEMPLATE_DIR = 'templates'
NAC_SRC_DIR = 'nac_src'

def load_yaml(filepath):
    with open(filepath, 'r') as f:
        return yaml.safe_load(f)

def load_source_files(src_dir):
    sources = {}
    for filename in os.listdir(src_dir):
        filepath = os.path.join(src_dir, filename)
        if os.path.isfile(filepath):
            with open(filepath, 'r') as f:
                sources[filename] = f.read()
    return sources

def generate_nac_manifests(topology, nac_policy):
    env = Environment(loader=FileSystemLoader(TEMPLATE_DIR))
    manifests = []

    rbac_template = env.get_template('nac/rbac.j2')
    manifests.append(rbac_template.render())

    source_codes = load_source_files(NAC_SRC_DIR)
    if source_codes:
        cm_template = env.get_template('nac/configmap-src.j2')
        manifests.append(cm_template.render(sources=source_codes))
    else:
        print("Warning: No source files found in nac_src/. ConfigMap will be empty.")

    job_template = env.get_template('nac/orchestrator-job.j2')
    manifests.append(job_template.render(
        policies=nac_policy.get('policies', []),
        nodes=topology.get('nodes', []),
        network_prefix=topology.get('network_prefix', '/ndn'),
        site_name=topology.get('site_name', 'default-site')
    ))
    return manifests

def save_output(manifests, filepath):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, 'w') as f:
        f.write("\n---\n".join(manifests))
    print(f"Generated NAC manifest at: {filepath}")

if __name__ == "__main__":
    topology_data = load_yaml(INPUT_TOPOLOGY)
    nac_policy_data = load_yaml(INPUT_POLICY)
    manifests = generate_nac_manifests(topology_data, nac_policy_data)
    save_output(manifests, OUTPUT_FILE)
