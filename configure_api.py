# configure_api.py
import sys
import pathlib

def main():
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <git_revision_for_api>")
        print("  <git_revision_for_api> can be a commit hash, branch name, or tag.")
        sys.exit(1)

    api_revision = sys.argv[1]
    placeholder = "@APICOMMIT@" # Make sure this matches your .wrap.in file

    # Define paths relative to this script's location (project root)
    project_root = pathlib.Path(__file__).parent.resolve()
    template_path = project_root / "binaryninja-api.wrap.in"
    subprojects_dir = project_root / "subprojects"
    output_wrap_path = subprojects_dir / "binaryninja-api.wrap"

    if not template_path.is_file():
        print(f"Error: Template file not found at {template_path}")
        sys.exit(1)

    try:
        # Read the template content
        content = template_path.read_text(encoding='utf-8')

        # Perform the replacement
        content = content.replace(placeholder, api_revision)

        # Ensure the subprojects directory exists
        subprojects_dir.mkdir(parents=True, exist_ok=True)

        # Write the new content to the output file
        output_wrap_path.write_text(content, encoding='utf-8')
        print(f"Successfully configured '{output_wrap_path}' to use API revision: {api_revision}")

    except Exception as e:
        print(f"Error processing wrap file: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()