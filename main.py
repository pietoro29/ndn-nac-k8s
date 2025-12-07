import yaml
from jinja2 import Environment, FileSystemLoader
import os

INPUT_FILE = 'input/topology.yaml'
OUTPUT_FILE = 'output/ndn-mesh.yaml'
TEMPLATE_DIR = 'templates'


def load_config(filepath):
    with open(filepath, 'r') as f:
        return yaml.safe_load(f)

def generate_manifests(config):
    env = Environment(loader=FileSystemLoader(TEMPLATE_DIR))
    manifests = []

    rbac_template = env.get_template('rbac.j2')
    manifests.append(rbac_template.render(config=config, security=config.get('security', {})))

    setup_job_template = env.get_template('setup-job.j2')
    manifests.append(setup_job_template.render(
        config=config,
        security=config.get('security', {}),
        nodes=config['nodes'],
        network_prefix=config['network_prefix'],
        site_name=config['site_name']
    ))

    nfd_conf_template = env.get_template('nfd.conf.j2')
    nfd_conf_content = nfd_conf_template.render()

    script_template = env.get_template('configmap-scripts.j2')
    manifests.append(script_template.render(
        config=config,
        nfd_conf=nfd_conf_content
        ))

    node_deployment_template = env.get_template('deployment.j2')
    nlsr_conf_template = env.get_template('nlsr.conf.j2')
    for node in config['nodes']:
        nlsr_conf_content = nlsr_conf_template.render(
            node=node,
            node_name=node['name'],
            neighbors=node['neighbors'],
            network_prefix=config['network_prefix'],
            site_name=config['site_name'],
            security=config.get('security', {})
        )
        rendered = node_deployment_template.render(
            node=node,
            network_prefix=config['network_prefix'],
            site_name=config['site_name'],
            nlsr_conf=nlsr_conf_content,
            security=config.get('security', {})
        )
        manifests.append(rendered)

    return manifests

def save_output(manifests, filepath):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, 'w') as f:
        f.write("\n---\n".join(manifests))
    print(f"Generated manifest at: {filepath}")

if __name__ == "__main__":
    config = load_config(INPUT_FILE)
    manifests = generate_manifests(config)
    save_output(manifests, OUTPUT_FILE)
