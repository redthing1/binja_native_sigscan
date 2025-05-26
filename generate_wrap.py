# generate_wrap.py
import sys
import pathlib

def main():
    if len(sys.argv) != 5: # script_name, template_path, output_path, api_commit_placeholder, actual_api_commit
        print(f"Usage: {sys.argv[0]} <template_path> <output_path> <placeholder_string> <api_commit_value>")
        sys.exit(1)

    template_file_path_str = sys.argv[1]
    output_file_path_str = sys.argv[2]
    placeholder = sys.argv[3]
    api_commit_value = sys.argv[4]

    template_path = pathlib.Path(template_file_path_str)
    output_path = pathlib.Path(output_file_path_str)

    try:
        # Read the template content
        content = template_path.read_text(encoding='utf-8')

        # Perform the replacement
        content = content.replace(placeholder, api_commit_value)

        # Ensure the output directory exists
        output_path.parent.mkdir(parents=True, exist_ok=True)

        # Write the new content to the output file
        output_path.write_text(content, encoding='utf-8')
    except Exception as e:
        print(f"Error generating wrap file: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()